from ddapp import lcmUtils
from ddapp import objectmodel as om
from ddapp import visualization as vis
from ddapp.utime import getUtime
from ddapp import transformUtils
from ddapp.debugVis import DebugData
from ddapp import ioUtils
from ddapp import robotstate
from ddapp import applogic as app

import os
import numpy as np
import drc as lcmdrc
import functools


def loadFootMeshes():
    meshDir = os.path.join(app.getDRCBase(), 'software/models/mit_gazebo_models/mit_robot/meshes')
    meshes = []
    for foot in ['l', 'r']:
        d = DebugData()
        d.addPolyData(ioUtils.readPolyData(os.path.join(meshDir, '%s_talus.stl' % foot), computeNormals=True))
        d.addPolyData(ioUtils.readPolyData(os.path.join(meshDir, '%s_foot.stl' % foot), computeNormals=True))
        meshes.append(d.getPolyData())
    return meshes


def getLeftFootMesh():
    return getFootMeshes()[0]


def getRightFootMesh():
    return getFootMeshes()[1]


def getLeftFootColor():
    return [1.0, 1.0, 0.0]


def getRightFootColor():
    return [0.33, 1.0, 0.0]

_footMeshes = None


def getDefaultStepParams():
    default_step_params = lcmdrc.footstep_params_t()
    default_step_params.step_speed = 1.0
    default_step_params.step_height = 0.05
    default_step_params.bdi_step_duration = 2.0
    default_step_params.bdi_sway_duration = 0.0
    default_step_params.bdi_lift_height = 0.05
    default_step_params.bdi_toe_off = 1
    default_step_params.bdi_knee_nominal = 0.0
    default_step_params.bdi_max_foot_vel = 0.0
    default_step_params.bdi_sway_end_dist = 0.02
    default_step_params.bdi_step_end_dist = 0.02
    default_step_params.mu = 1.0
    return default_step_params


def getFootMeshes():
    global _footMeshes
    if not _footMeshes:
        _footMeshes = loadFootMeshes()
    return _footMeshes


def getFootstepsFolder():
    obj = om.findObjectByName('footstep plan')
    if obj is None:
        obj = om.getOrCreateContainer('footstep plan')
        #om.collapse(obj)
    return obj


class FootstepsDriver(object):
    def __init__(self, jc):
        self.lastFootstepPlanMessage = None
        self.goalSteps = None
        self.has_plan = False
        self._setupSubscriptions()
        self.jc = jc

    def _setupSubscriptions(self):
        lcmUtils.addSubscriber('FOOTSTEP_PLAN_RESPONSE', lcmdrc.footstep_plan_t, self.onFootstepPlan)

    def clearFootstepPlan(self):
        self.has_plan = False
        folder = getFootstepsFolder()
        om.removeFromObjectModel(folder)

    def onFootstepPlan(self, msg):
        self.clearFootstepPlan()
        self.has_plan = True
        planFolder = getFootstepsFolder()

        for i, footstep in enumerate(msg.footsteps[2:]):
            trans = footstep.pos.translation
            trans = [trans.x, trans.y, trans.z]
            quat = footstep.pos.rotation
            quat = [quat.w, quat.x, quat.y, quat.z]
            footstepTransform = transformUtils.transformFromPose(trans, quat)

            if footstep.is_right_foot:
                mesh = getRightFootMesh()
                color = getRightFootColor()
            else:
                mesh = getLeftFootMesh()
                color = getLeftFootColor()

            obj = vis.showPolyData(mesh, 'step %d' % i, color=color, alpha=1.0, parent=planFolder)
            frameObj = vis.showFrame(footstepTransform, 'frame', parent=obj, scale=0.3, visible=False)
            frameObj.onTransformModifiedCallback = functools.partial(self.onStepModified, i)
            obj.actor.SetUserTransform(footstepTransform)

        self.lastFootstepPlanMessage = msg

    def createWalkingGoal(self, model):
        distanceForward = 1.0

        t1 = model.getLinkFrame('l_foot')
        t2 = model.getLinkFrame('r_foot')
        pelvisT = model.getLinkFrame('pelvis')

        xaxis = [1.0, 0.0, 0.0]
        pelvisT.TransformVector(xaxis, xaxis)
        xaxis = np.array(xaxis)
        zaxis = np.array([0.0, 0.0, 1.0])
        yaxis = np.cross(zaxis, xaxis)
        xaxis = np.cross(yaxis, zaxis)

        stancePosition = np.array(t2.GetPosition()) + np.array(t1.GetPosition()) / 2.0

        footHeight = 0.0817

        t = transformUtils.getTransformFromAxes(xaxis, yaxis, zaxis)
        t.PostMultiply()
        t.Translate(stancePosition)
        t.Translate([0.0, 0.0, -footHeight])
        t.Translate(xaxis*distanceForward)

        frameObj = vis.showFrame(t, 'walking goal')
        frameObj.setProperty('Edit', True)

        frameObj.onTransformModifiedCallback = self.onWalkingGoalModified
        self.sendFootstepPlanRequest()

    def createGoalSteps(self, model):
        distanceForward = 1.0

        fr = model.getLinkFrame('l_foot')
        fl = model.getLinkFrame('r_foot')
        pelvisT = model.getLinkFrame('pelvis')

        xaxis = [1.0, 0.0, 0.0]
        pelvisT.TransformVector(xaxis, xaxis)
        xaxis = np.array(xaxis)
        zaxis = np.array([0.0, 0.0, 1.0])
        yaxis = np.cross(zaxis, xaxis)
        xaxis = np.cross(yaxis, zaxis)

        numGoalSteps = 3
        is_right_foot = True
        self.goalSteps = []
        for i in range(numGoalSteps):
            t = transformUtils.getTransformFromAxes(xaxis, yaxis, zaxis)
            t.PostMultiply()
            if is_right_foot:
                t.Translate(fr.GetPosition())
            else:
                t.Translate(fl.GetPosition())
            t.Translate(xaxis*distanceForward)
            distanceForward += 0.15
            is_right_foot = not is_right_foot
            step = lcmdrc.footstep_t()
            step.pos = transformUtils.positionMessageFromFrame(t)
            step.is_right_foot = is_right_foot
            step.params = getDefaultStepParams()
            self.goalSteps.append(step)
        request = self.constructFootstepPlanRequest()
        request.num_goal_steps = len(self.goalSteps)
        request.goal_steps = self.goalSteps
        lcmUtils.publish('FOOTSTEP_PLAN_REQUEST', request)
        return request

    def onStepModified(self, ndx, frameObj):
        self.lastFootstepPlanMessage.footsteps[ndx+2].pos = transformUtils.positionMessageFromFrame(frameObj.transform)
        self.lastFootstepPlanMessage.footsteps[ndx+2].fixed_x = True
        self.lastFootstepPlanMessage.footsteps[ndx+2].fixed_y = True
        self.lastFootstepPlanMessage.footsteps[ndx+2].fixed_yaw = True
        self.sendUpdatePlanRequest()

    def sendUpdatePlanRequest(self):
        msg = self.constructFootstepPlanRequest()
        msg.num_existing_steps = self.lastFootstepPlanMessage.num_steps
        msg.existing_steps = self.lastFootstepPlanMessage.footsteps
        lcmUtils.publish('FOOTSTEP_PLAN_REQUEST', msg)
        return msg

    def onWalkingGoalModified(self, frameObj):
        pos, wxyz = transformUtils.poseFromTransform(frameObj.transform)
        self.sendFootstepPlanRequest()

    def constructFootstepPlanRequest(self):

        msg = lcmdrc.footstep_plan_request_t()
        msg.utime = getUtime()
        pose = self.jc.getPose('q_end')
        state_msg = robotstate.drakePoseToRobotState(pose)
        msg.initial_state = state_msg

        goalObj = om.findObjectByName('walking goal')
        if goalObj:
            msg.goal_pos = transformUtils.positionMessageFromFrame(goalObj.transform)
        else:
            msg.goal_pos = transformUtils.positionMessageFromFrame(transformUtils.transformFromPose((0,0,0), (1,0,0,0)))
        msg.params = lcmdrc.footstep_plan_params_t()
        msg.params.max_num_steps = 30
        msg.params.min_num_steps = 0
        msg.params.min_step_width = 0.21
        msg.params.nom_step_width = 0.25
        msg.params.max_step_width = 0.4
        msg.params.nom_forward_step = 0.15
        msg.params.max_forward_step = 0.45
        msg.params.ignore_terrain = False
        msg.params.planning_mode = msg.params.MODE_AUTO
        msg.params.behavior = msg.params.BEHAVIOR_BDI_STEPPING
        msg.params.map_command = 2
        msg.params.leading_foot = msg.params.LEAD_AUTO
        msg.default_step_params = getDefaultStepParams()

        return msg

    def sendFootstepPlanRequest(self):
        msg = self.constructFootstepPlanRequest()
        lcmUtils.publish('FOOTSTEP_PLAN_REQUEST', msg)
        return msg

    def sendStopWalking(self):
        msg = lcmdrc.plan_control_t()
        msg.utime = getUtime()
        msg.control = lcmdrc.plan_control_t.TERMINATE
        lcmUtils.publish('STOP_WALKING', msg)

    def commitFootsepPlan(self):
        if not self.has_plan:
            print "I don't have a footstep plan to commit"
            return
        self.lastFootstepPlanMessage.utime = getUtime()
        lcmUtils.publish('COMMITTED_FOOTSTEP_PLAN', self.lastFootstepPlanMessage)


def init(jc):
    global driver
    driver = FootstepsDriver(jc)

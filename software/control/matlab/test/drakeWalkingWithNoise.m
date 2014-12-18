function drakeWalkingWithNoise()
%NOTEST
% tests forward walking on flat terrain with state noise, actuator
% delays, and inertial/damping errors

use_bullet = false;

addpath(fullfile(getDrakePath,'examples','ZMP'));

plot_comtraj = false;
%navgoal = [0.5*randn();0.5*randn();0;0;0;pi/2*randn()];
navgoal = [1;0;0;0;0;0];

% silence some warnings
warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits')

% construct robot model---one for simulation, one for control (to test
% model discrepencies)
options.floating = true;
options.ignore_friction = true;
options.dt = 0.002;
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);
r = r.removeCollisionGroupsExcept({'heel','toe'});
r = compile(r);
v = r.constructVisualizer;
v.display_dt = 0.05;

% ******************* BEGIN ADJUSTABLE ************************************
% *************************************************************************
options.inertia_error = 0.05; % standard deviation for inertia noise (percentage of true inertia)
options.damping_error = 0.05; % standard deviation for damping noise (percentage of true joint damping)
% ******************* END ADJUSTABLE **************************************

rctrl = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);

% set initial state to fixed point
load(strcat(getenv('DRC_PATH'),'/control/matlab/data/atlas_fp.mat'));
r = r.setInitialState(xstar);
rctrl = rctrl.setInitialState(xstar);

nq = getNumPositions(r);
nu = getNumInputs(r);

x0 = xstar;

% create footstep and ZMP trajectories
footstep_planner = StatelessFootstepPlanner();
request = drc.footstep_plan_request_t();
request.utime = 0;
request.initial_state = r.getStateFrame().lcmcoder.encode(0, x0);
request.goal_pos = encodePosition3d(navgoal);
request.num_goal_steps = 0;
request.num_existing_steps = 0;
request.params = drc.footstep_plan_params_t();
request.params.max_num_steps = 30;
request.params.min_num_steps = 2;
request.params.min_step_width = 0.2;
request.params.nom_step_width = 0.24;
request.params.max_step_width = 0.3;
request.params.nom_forward_step = 0.2;
request.params.max_forward_step = 0.4;
request.params.planning_mode = request.params.MODE_AUTO;
request.params.behavior = request.params.BEHAVIOR_WALKING;
request.params.map_mode = drc.footstep_plan_params_t.FOOT_PLANE;
request.params.leading_foot = request.params.LEAD_AUTO;
request.default_step_params = drc.footstep_params_t();
request.default_step_params.step_speed = 0.5;
request.default_step_params.drake_min_hold_time = 2.0;
request.default_step_params.step_height = 0.05;
request.default_step_params.mu = 1.0;
request.default_step_params.constrain_full_foot_pose = false;

footstep_plan = footstep_planner.plan_footsteps(r, request);

walking_planner = StatelessWalkingPlanner();
request = drc.walking_plan_request_t();
request.initial_state = r.getStateFrame().lcmcoder.encode(0, x0);
request.footstep_plan = footstep_plan.toLCM();
walking_plan = walking_planner.plan_walking(r, request, true);
walking_ctrl_data = walking_planner.plan_walking(r, request, false);

% No-op: just make sure we can cleanly encode and decode the plan as LCM
tic;
walking_ctrl_data = WalkingControllerData.from_walking_plan_t(walking_ctrl_data.toLCM());
fprintf(1, 'control data lcm code/decode time: %f\n', toc);

ts = walking_plan.ts;
T = ts(end);

ctrl_data = QPControllerData(true,struct(...
  'acceleration_input_frame',drcFrames.AtlasCoordinates(r),...
  'D',-getAtlasNominalCOMHeight()/9.81*eye(2),... % assumed COM height
  'Qy',eye(2),...
  'S',walking_ctrl_data.S,... % always a constant
  's1',walking_ctrl_data.s1,...
  's2',walking_ctrl_data.s2,...
  's1dot',walking_ctrl_data.s1dot,...
  's2dot',walking_ctrl_data.s2dot,...
  'x0',[walking_ctrl_data.zmptraj.eval(T);0;0],...
  'u0',zeros(2,1),...
	'qtraj',x0(1:nq),...
	'comtraj',walking_ctrl_data.comtraj,...
  'link_constraints',walking_ctrl_data.link_constraints, ...
  'support_times',walking_ctrl_data.support_times,...
  'supports',walking_ctrl_data.supports,...
  'mu',walking_ctrl_data.mu,...
  'ignore_terrain',walking_ctrl_data.ignore_terrain,...
  'y0',walking_ctrl_data.zmptraj,...
  'plan_shift',zeros(3,1),...
  'constrained_dofs',[findJointIndices(r,'arm');findJointIndices(r,'neck')]));


% ******************* BEGIN ADJUSTABLE ************************************
% *************************************************************************
options.dt = 0.003;
options.slack_limit = 30.0;
options.w_qdd = 0.001*ones(nq,1);
options.w_grf = 0;
options.w_slack = 0.001;
options.W_kdot = 0*eye(3);
options.contact_threshold = 0.005;
options.debug = false;
options.use_mex = true;
options.solver = 0;
options.use_bullet = use_bullet;

% ******************* END ADJUSTABLE **************************************

% instantiate QP controller
qp = QPController(rctrl,{},ctrl_data,options);

% ******************* BEGIN ADJUSTABLE ************************************
% *************************************************************************
options.delay_steps = 1;
options.use_input_frame = true;
% ******************* END ADJUSTABLE **************************************

% cascade qp controller with delay block
delayblk = DelayBlock(r,options);
sys = cascade(qp,delayblk);

% ******************* BEGIN ADJUSTABLE ************************************
% *************************************************************************
options.noise_model = struct();
% position noise
options.noise_model(1).ind = (1:nq)';
options.noise_model(1).type = 'gauss_markov';
options.noise_model(1).params = struct('std',0.0001);
% velocity noise
options.noise_model(2).ind = (nq+(1:nq))';
options.noise_model(2).type = 'white_noise';
options.noise_model(2).params = struct('std',0.0025);
% ******************* END ADJUSTABLE **************************************

% cascade robot with noise block
noiseblk = StateCorruptionBlock(r,options);
rnoisy = cascade(r,noiseblk);

% cascade footstep plan shift block
fs = FootstepPlanShiftBlock(rctrl,ctrl_data,options);
rnoisy = cascade(rnoisy,fs);

% feedback QP controller with atlas
ins(1).system = 1;
ins(1).input = 2;
ins(2).system = 1;
ins(2).input = 3;
outs(1).system = 2;
outs(1).output = 1;
sys = mimoFeedback(sys,rnoisy,[],[],ins,outs);
clear ins;

% feedback foot contact detector with QP/atlas
options.use_lcm=false;
fc = atlasControllers.FootContactBlock(r,ctrl_data,options);
ins(1).system = 2;
ins(1).input = 1;
sys = mimoFeedback(fc,sys,[],[],ins,outs);
clear ins;  

pd = atlasControllers.IKPDBlock(r,ctrl_data,options);
ins(1).system = 1;
ins(1).input = 1;
sys = mimoFeedback(pd,sys,[],[],ins,outs);
clear ins;

qt = atlasControllers.QTrajEvalBlock(rctrl,ctrl_data,options);
sys = mimoFeedback(qt,sys,[],[],[],outs);

S=warning('off','Drake:DrakeSystem:UnsupportedSampleTime');
output_select(1).system=1;
output_select(1).output=1;
sys = mimoCascade(sys,v,[],[],output_select);
warning(S);
traj = simulate(sys,[0 T],[x0;0*x0;zeros((options.delay_steps+1)*nu,1)]);
playback(v,traj,struct('slider',true));

com_err = 0; % x,y error
foot_err = 0;
pelvis_sway = 0;
rfoot_idx = findLinkInd(r,'r_foot');
lfoot_idx = findLinkInd(r,'l_foot');
rfoottraj = walking_ctrl_data.link_constraints(1).traj;
lfoottraj = walking_ctrl_data.link_constraints(2).traj;
for i=1:length(ts)
  x=traj.eval(ts(i));
  q=x(1:getNumPositions(r));
  kinsol = doKinematics(r,q);
  com(:,i)=getCOM(r,q);
  com_err = com_err + norm(walking_ctrl_data.comtraj.eval(ts(i)) - com(1:2,i))^2;

  rfoot_pos = forwardKin(r,kinsol,rfoot_idx,[0;0;0]);
  rfoot_des = rfoottraj.eval(ts(i));
  lfoot_pos = forwardKin(r,kinsol,lfoot_idx,[0;0;0]);
  lfoot_des = lfoottraj.eval(ts(i));
  foot_err = foot_err + norm(rfoot_des(1:3) - rfoot_pos)^2 + norm(lfoot_des(1:3) - lfoot_pos)^2;

  pelvis_pos = forwardKin(r,kinsol,rfoot_idx,[0;0;0],1);
  pelvis_sway = pelvis_sway + sum(abs(pelvis_pos(4:6)));
end
com_err = sqrt(com_err / length(ts))
foot_err = sqrt(foot_err / length(ts))
pelvis_sway

if plot_comtraj
  figure(2);
  subplot(3,1,1);
  plot(ts,com(1,:),'r');
  subplot(3,1,2);
  plot(ts,com(2,:),'r');
  subplot(3,1,3); hold on;
  plot(com(1,:),com(2,:),'r');
end

if com_err > 0.15
  error('drakeWalking unit test failed: error is too large');
  navgoal
end

end

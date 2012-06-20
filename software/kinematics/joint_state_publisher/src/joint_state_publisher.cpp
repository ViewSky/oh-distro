// file: joint_angle_publisher.cpp
//
// Communicates to gazebo/robot API to get joint angles and then 
// publishes them on JOINT_ANGLES lcm channel.
// The publisher requires access to the urdf robot model. 
// At startup, the process creates a subcription to ROBOT_MODEL lcm channel,
// waits until it receives the urdf string and then unsubcribes from this
// channel.
//


#include <iostream>
#include <stdint.h> 
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <urdf/model.h>
#include <lcm/lcm-cpp.hpp>
#include "lcmtypes/drc_lcmtypes.hpp"

namespace joint_state_publisher {
  
class RobotModel {
 public:
   lcm::LCM lcm;
   std::string urdf_xml_string; 
   std::vector<std::string> joint_names_;
 };

void onMessage(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  drc::robot_urdf_t* msg, RobotModel* robot) {
  // Received robot urdf string. Store it internally and get all available joints.
  robot->urdf_xml_string = msg->urdf_xml_string;
  std::cout<<"Received urdf_xml_string, storing it internally as a param"<<std::endl;

  urdf::Model robot_model; 
  if (!robot_model.initString( msg->urdf_xml_string))
  {std::cerr << "ERROR: Could not generate robot model" << std::endl;}

  typedef std::map<std::string, boost::shared_ptr<urdf::Joint> > joints_mapType;
  for( joints_mapType::const_iterator it = robot_model.joints_.begin(); it!=robot_model.joints_.end(); it++)
  { 
	  if(it->second->type!=6) // All joints that not of the type FIXED.
               robot->joint_names_.push_back(it->first);
  }
 }//end onMessage

}//end namespace

int main(int argc, char ** argv)
{
  // Creates a subcription to ROBOT_MODEL channel to download the robot_model.
  // The received robot_model is instantiated into a RobotModel object. This object
  // contains the urdf_string as well as a list of joint names that are available on
  // the robot.
  joint_state_publisher::RobotModel* robot = new joint_state_publisher::RobotModel;

  lcm::Subscription* robot_model_subcription_;
  robot_model_subcription_ = robot->lcm.subscribeFunction("ROBOT_MODEL", joint_state_publisher::onMessage, robot);

  while(robot->lcm.handle()==-1);// wait for one message, wait until you get a success.
  robot->lcm.unsubscribe(robot_model_subcription_); // Stop listening to ROBOT_MODEL.

   
  std::cout<< robot->urdf_xml_string <<std::endl;
  std::cout<< "Number of Joints: " << robot->joint_names_.size() <<std::endl;

 
// Start reading joint angles via gazebo/robot API for the joints mentioned in the URDF
// description and start publishing them 
  lcm::LCM lcm;
    if(!lcm.good())
        return 1;


  drc::joint_state_t message;


// Get joint angles using  gazebo/robot API for all joints in robot->joint_names_
  while(true)
  {
    //message.timestamp = time(NULL);
    struct timeval tv;
    gettimeofday (&tv, NULL);
    message.timestamp = (int64_t) tv.tv_sec * 1000000 + tv.tv_usec; // TODO: replace with bot_timestamp_now() from bot_core
    message.num_joints = robot->joint_names_.size();

    for(std::vector<std::string>::size_type i = 0; i != robot->joint_names_.size(); i++) 
    {
      //std::cout<< robot->joint_names_[i] <<std::endl;
      message.joint_name.push_back(robot->joint_names_[i]);
      //TODO: INSERT ROBOT/GAZEBO API HERE?
      message.position.push_back(0);
      message.velocity.push_back(0);
      message.effort.push_back(0);
    }

    // Publishing joint angles.
    lcm.publish("JOINT_STATES", &message);
  
    //TODO: better Timing.
    usleep(50000); // publish at 20 hz.
   } 

   delete robot; 
   return 0;
 
}



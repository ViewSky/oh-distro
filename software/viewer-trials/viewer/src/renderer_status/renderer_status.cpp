#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <map>
#include <list>
#include <utility>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <string>
#include <deque>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/freeglut.h>

#include <bot_vis/bot_vis.h>
#include <bot_core/bot_core.h>

#include "lcmtypes/drc_affordance_collection_t.h"
#include "lcmtypes/drc_atlas_status_t.h"
#include "lcmtypes/drc_bandwidth_stats_t.h"
#include "lcmtypes/drc_controller_status_t.h"
#include "lcmtypes/drc_estimated_biases_t.h"
#include "lcmtypes/drc_foot_contact_estimate_t.h"
#include "lcmtypes/drc_frequency_t.h"
#include "lcmtypes/drc_grasp_opt_status_t.h"
#include "lcmtypes/drc_hand_tactile_state_t.h"
#include "lcmtypes/drc_planner_config_t.h"
#include "lcmtypes/drc_system_status_t.h"
#include "lcmtypes/drc_robot_state_t.h"
#include "lcmtypes/drc_utime_t.h"

using namespace std;
#define RENDERER_NAME "System_Status"
const char* PARAM_STATUS_0 = "Network";
const char* PARAM_STATUS_1 = "Motion Estimation";
const char* PARAM_STATUS_2 = "Tracking";
const char* PARAM_STATUS_3 = "Control";
const char* PARAM_STATUS_4 = "Grasping";
const char* PARAM_STATUS_5 = "Unused";
const char* PARAM_STATUS_6 = "Planning (base)";
const char* PARAM_STATUS_7 = "Planning (robot)";
const char* PARAM_STATUS_8 = "Fall Detector";
#define NUMBER_OF_SYSTEMS 9
#define MAXIMUM_N_OF_LINES 80

const char* PARAM_IMPORTANT = "Important";
const char* PARAM_MODE = "Mode";

const bool PARAM_STATUS_0_DEFAULT = false;
const bool PARAM_STATUS_1_DEFAULT = false;
const bool PARAM_STATUS_2_DEFAULT = false;
const bool PARAM_STATUS_3_DEFAULT = false;
const bool PARAM_STATUS_4_DEFAULT = false;
const bool PARAM_STATUS_5_DEFAULT = false;
const bool PARAM_STATUS_6_DEFAULT = false;
const bool PARAM_STATUS_7_DEFAULT = false;
const bool PARAM_STATUS_8_DEFAULT = false;
const bool PARAM_IMPORTANT_DEFAULT = false;

float safe_colors[NUMBER_OF_SYSTEMS][3] = {
  { 0.2000, 0.6274, 0.1725 },  // darker green
  { 0.6509, 0.8078, 0.8901 }, // light blue
  { 0.6980, 0.8740, 0.5411 }, // light green
  { 0.3015, 0.6005, 0.7058 }, // dark blue ... too dark
  { 0.9843, 0.6039, 0.6000 }, // orange
  { 0.8901, 0.3519, 0.3598 },  // rec ... too dark
  { 0.9921, 0.7490, 0.4352 }, // bright orange
  { 0.6156, 0.4392, 0.7039 }, // purple
  { 1.0, 0.5, 0.5} , // salmon  
};



const char* PARAM_SHADING = "Shading";
const bool PARAM_SHADING_DEFAULT = true;
const char* PARAM_RATES = "Message Frequency";
const bool PARAM_RATES_DEFAULT= true;

#define ERR(fmt, ...) do { \
  fprintf(stderr, "["__FILE__":%d Error: ", __LINE__); \
  fprintf(stderr, fmt, ##__VA_ARGS__); \
} while (0)


typedef struct
{
  // state of the planner
  double desired_ee_arc_speed;
  double desired_joint_speed;
  std::string planner_name;
  double plan_execution_time;

} CurrentPlan;


typedef struct
{
  BotRenderer renderer;
  BotViewer *viewer;
  BotGtkParamWidget *pw;  

  lcm_t *lcm;
  int64_t last_utime;
  int64_t last_grasp_opt_status_utime;
  int64_t controller_utime;
  int8_t controller_state;
  
  int64_t atlas_utime; // time from the atlas driver
  int atlas_state;  
  
  drc_system_status_t_subscription_t *status_sub;
  
  // Information to be displayed:
  double pitch, head_pitch;
  double roll, head_roll;
  double height, head_height;
  int naffs;
  double speed, head_speed, cmd_speed;
  
  deque<drc_system_status_t *> * sys_deque;
  vector< deque<drc_system_status_t *> * > deques;
  
  // used for multidimensional lists:
  vector<string> msgchannels;
  bool param_status[NUMBER_OF_SYSTEMS];
  bool param_important;
  bool shading;
  int visability;
  // for custom checkboxes
  GtkWidget* vbox;   
    
  int64_t frequency_utime;
  std::vector<int> frequency_list;
  std::vector< int8_t> channel_list;
  int8_t real_time_percent;
  
  float left_foot_contact; // foot contact
  float right_foot_contact;
  float foot_left[3];
  float foot_right[3];
  float foot_spacing;
  
  float left_hand_contact;
  float right_hand_contact;
  float l_foot_force_z;
  float r_foot_force_z;
  
  float estimated_biases[3];
  bool estimated_biases_converged;
  
  int estimated_latency_ms;
  int target_bps;

  CurrentPlan current_plan;
  
} RendererSystemStatus;

enum {
  MODE_FULL, // text visability
  MODE_FADE,  
  MODE_NONE,  
};

static void
on_drc_system_status(const lcm_recv_buf_t *rbuf,
    const char *channel, const drc_system_status_t *msg, void *user)
{
  RendererSystemStatus *self = (RendererSystemStatus*) user;
  
  drc_system_status_t* recd = drc_system_status_t_copy(msg);
  // NB: overwrite the incoming timestamp as that might have come from a simulated clock
  recd->utime = bot_timestamp_now();
  
  if ( msg->system >= NUMBER_OF_SYSTEMS ){
    std::cout << "status from system ["<< (int)msg->system << "] will be ignored. Code supports ["<< NUMBER_OF_SYSTEMS <<"] subsystems\n"; 
    return;
  }
  
  self->sys_deque->push_back(recd);
  if( self->sys_deque->size() > MAXIMUM_N_OF_LINES){
    self->sys_deque->pop_front();
  }

  // If we dont know which system this belongs to, remove it
  int w = (int) msg->system;
  if (w < NUMBER_OF_SYSTEMS+2){
    self->deques[ w ]->push_back( recd );
    if( self->deques[ w ]->size() > MAXIMUM_N_OF_LINES){
    self->deques[ w ]->pop_front();
    }
  }
  
  /*stringstream ss;
  ss << "Deque Sizes: ";
  for (int i=0;i<3;i++){
    ss << self->deques[i]->size() << ", ";
  }
  cout << ss.str() <<"\n";*/
}


static void
on_estimated_bias(const lcm_recv_buf_t * buf, const char *channel, const drc_estimated_biases_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->estimated_biases[0] = msg->x;
  self->estimated_biases[1] = msg->y;
  self->estimated_biases[2] = msg->z;
  if (msg->mode ==0){
    self->estimated_biases_converged = FALSE;
  }else{
    self->estimated_biases_converged = TRUE;
  }
}

static void
on_pose_body(const lcm_recv_buf_t * buf, const char *channel, const bot_core_pose_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  double rpy_in[3];
  bot_quat_to_roll_pitch_yaw (msg->orientation, rpy_in) ;
  self->roll = rpy_in[0]*180/M_PI;
  self->pitch = rpy_in[1]*180/M_PI;
  //self->yaw = rpy_in[2]*180/M_PI;

  self->height = msg->pos[2]; 
  self->speed = sqrt( msg->vel[0]*msg->vel[0] + msg->vel[1]*msg->vel[1] + msg->vel[2]*msg->vel[2] );
}

static void
on_pose_head(const lcm_recv_buf_t * buf, const char *channel, const bot_core_pose_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  double rpy_in[3];
  bot_quat_to_roll_pitch_yaw (msg->orientation, rpy_in) ;
  self->head_roll = rpy_in[0]*180/M_PI;
  self->head_pitch = rpy_in[1]*180/M_PI;
  //self->yaw = rpy_in[2]*180/M_PI;
  
  self->head_height = msg->pos[2];
  self->head_speed = sqrt( msg->vel[0]*msg->vel[0] + msg->vel[1]*msg->vel[1] + msg->vel[2]*msg->vel[2] );
}

static void
on_foot_left(const lcm_recv_buf_t * buf, const char *channel, const bot_core_pose_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->foot_left[0]=msg->pos[0];
  self->foot_left[1]=msg->pos[1];
  self->foot_left[2]=msg->pos[2];
  self->foot_spacing = sqrt( pow(self->foot_left[0]-self->foot_right[0],2) + pow(self->foot_left[1]-self->foot_right[1],2) );
}
static void
on_foot_right(const lcm_recv_buf_t * buf, const char *channel, const bot_core_pose_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->foot_right[0]=msg->pos[0];
  self->foot_right[1]=msg->pos[1];
  self->foot_right[2]=msg->pos[2];
  self->foot_spacing = sqrt( pow(self->foot_left[0]-self->foot_right[0],2) + pow(self->foot_left[1]-self->foot_right[1],2) );
}


static void
on_affordance_collection(const lcm_recv_buf_t * buf, const char *channel, const drc_affordance_collection_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->naffs = msg->naffs;
}

static void
on_robot_state(const lcm_recv_buf_t * buf, const char *channel, const drc_robot_state_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->last_utime = msg->utime;
  self->l_foot_force_z = msg->force_torque.l_foot_force_z;
  self->r_foot_force_z = msg->force_torque.r_foot_force_z;
}

static void
on_controller_status(const lcm_recv_buf_t * buf, const char *channel, const drc_controller_status_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->controller_state = msg->state;
  self->controller_utime = msg->utime;
}

static void
on_utime(const lcm_recv_buf_t * buf, const char *channel, const drc_utime_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->last_utime = msg->utime;
}

static void
on_frequency(const lcm_recv_buf_t * buf, const char *channel, const drc_frequency_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->frequency_utime =  msg->utime;
  self->frequency_list.clear();
  self->channel_list.clear(); 
  for (size_t i=0;i <msg->num; i++){
  self->frequency_list.push_back( (int16_t) msg->frequency[i] );
  self->channel_list.push_back( (int8_t) msg->channel[i] );
  }
  //std::cout << "freqs recevied\n";
  
  self->real_time_percent = msg->real_time_percent;
}

static void
on_foot_contact(const lcm_recv_buf_t * buf, const char *channel, const drc_foot_contact_estimate_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->left_foot_contact = msg->left_contact;
  self->right_foot_contact = msg->right_contact;
}

static void
on_atlas_status(const lcm_recv_buf_t * buf, const char *channel, const drc_atlas_status_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;
  self->atlas_state = msg->behavior;
  self->atlas_utime = msg->utime; 
}

static void
on_grasp_opt_status(const lcm_recv_buf_t * buf, const char *channel, const drc_grasp_opt_status_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data; 
  self->last_grasp_opt_status_utime = bot_timestamp_now();//msg->utime; use system time hear to maintain heart beat from drake  
}

static void
on_tactile_state(const lcm_recv_buf_t * buf, const char *channel, const drc_hand_tactile_state_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data; 
  
  if ( ( channel == "IROBOT_L_HAND_TACTILE_STATE") || ( channel == "SANDIA_L_HAND_TACTILE_STATE") ){
    self->left_hand_contact = msg->signal;
  }else if( ( channel == "IROBOT_R_HAND_TACTILE_STATE") || ( channel == "SANDIA_R_HAND_TACTILE_STATE") ){
    self->right_hand_contact = msg->signal;
  }
}


static void
on_bw_stats(const lcm_recv_buf_t * buf, const char *channel, const drc_bandwidth_stats_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data; 
  self->estimated_latency_ms = msg->estimated_latency_ms;
  self->target_bps = msg->target_bps;
}


static string get_planner_string(int16_t mode){
  if( mode ==  DRC_PLANNER_CONFIG_T_IKSEQUENCE_ON){
    return string("IK On");      
  }else if( mode ==  DRC_PLANNER_CONFIG_T_IKSEQUENCE_OFF){
    return string("IK Off");      
  }else if( mode ==  DRC_PLANNER_CONFIG_T_TELEOP){
    return string("Teleop");            
  }else if( mode ==  DRC_PLANNER_CONFIG_T_FIXEDJOINTS){
    return string("Fixed Joints");            
  }
}

static void
on_planner_config(const lcm_recv_buf_t * buf, const char *channel, const drc_planner_config_t *msg, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*) user_data; 
  self->current_plan.desired_ee_arc_speed = msg->desired_ee_arc_speed;
  self->current_plan.desired_joint_speed = msg->desired_joint_speed;
  self->current_plan.plan_execution_time = msg->plan_execution_time;
  
  if (msg->active_planner == DRC_PLANNER_CONFIG_T_REACHING_PLANNER){
    self->current_plan.planner_name = string("Reaching w ") + get_planner_string(msg->reaching_mode);
  }else if (msg->active_planner == DRC_PLANNER_CONFIG_T_ENDPOSE_PLANNER){
    self->current_plan.planner_name = "Endpose"; 
  }else if (msg->active_planner == DRC_PLANNER_CONFIG_T_POSTURE_PLANNER){
    self->current_plan.planner_name = "Posture"; 
  }else if (msg->active_planner == DRC_PLANNER_CONFIG_T_MANIPULATION_PLANNER){
    self->current_plan.planner_name = string("Manip w ") + get_planner_string(msg->reaching_mode);
  }else{
    self->current_plan.planner_name = "Unknown planner"; 
  }
  
}




////////////////////////////////////////////////////////////////////////////////
// ------------------------------ Drawing Functions ------------------------- //
////////////////////////////////////////////////////////////////////////////////

static void _draw(BotViewer *viewer, BotRenderer *r){
  RendererSystemStatus *self = (RendererSystemStatus*)r;

  glPushAttrib (GL_ENABLE_BIT);
  glEnable (GL_BLEND);
  glDisable (GL_DEPTH_TEST);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  int gl_width = (GTK_WIDGET(viewer->gl_area)->allocation.width);
  int gl_height = (GTK_WIDGET(viewer->gl_area)->allocation.height);

  // transform into window coordinates, where <0, 0> is the top left corner
  // of the window and <viewport[2], viewport[3]> is the bottom right corner
  // of the window
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, gl_width, 0, gl_height);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glTranslatef(0, gl_height, 0);
  glScalef(1, -1, 1);

  //void *font = GLUT_BITMAP_8_BY_13;
  void *font = GLUT_BITMAP_9_BY_15;
  int line_height = 14;
  
  // Printf the frequency_list:
  if (bot_gtk_param_widget_get_bool(self->pw, PARAM_RATES)){
    if (self->frequency_list.size()>0){
	double x=0;
	double y=0;
	for (size_t i=0; i <self->frequency_list.size() ; i++) {
	  char line[80];
	  ///std::cout <<  self->frequency_list[i] << "\n";
      
      std::string chan="";
      int happy_freq_thres =99999;
      if (self->channel_list[i] == DRC_FREQUENCY_T_EST_ROBOT_STATE ){ chan = "EST_ROBOT_STATE"; happy_freq_thres = 300;
      }else if (self->channel_list[i] == DRC_FREQUENCY_T_ATLAS_COMMAND ){ chan = "ATLAS_COMMAND"; happy_freq_thres = 300;
      }else if (self->channel_list[i] == DRC_FREQUENCY_T_CAMERA ){ chan = "CAMERA"; happy_freq_thres = 3;
      }else if (self->channel_list[i] == DRC_FREQUENCY_T_CAMERA_LHAND ){ chan = "CAMERA_LHAND"; happy_freq_thres = -1; 
      }else if (self->channel_list[i] == DRC_FREQUENCY_T_CAMERA_RHAND ){ chan = "CAMERA_RHAND"; happy_freq_thres = -1;
      }else if (self->channel_list[i] == DRC_FREQUENCY_T_SCAN ){ chan = "SCAN"; happy_freq_thres = 35;
      }else { chan = "UNKNOWN"; }
      
      snprintf(line, 70,  "%03d %s", self->frequency_list[i], chan.c_str());
      if (self->channel_list[i] == DRC_FREQUENCY_T_SCAN ){
  	  float elapsed_time =  (self->last_utime - self->frequency_utime)*1E-6;
      snprintf(line, 70,  "%03d %s [%.1f]", self->frequency_list[i], chan.c_str(), elapsed_time);
      }
      
	  x = 0 ;// hind * 150 + 120;
   	  y = gl_height + (-i - 9) * line_height;
      if ( self->frequency_list[i] > happy_freq_thres ){
  	  glColor3f(  0.0, 0.8, 0.0 );
      }else{
  	  glColor3f(  0.8, 0.0, 0.0 );
      }
	  glRasterPos2f(x, y);
	  glutBitmapString(font, (unsigned char*) line);
	}
    }
  }
  
  
  char line[80];
  snprintf(line, 70,  "ee speed %2.1f cm/s", self->current_plan.desired_ee_arc_speed*100 );
  glColor3f(  0.0, 0.0, 0.8 );
  glRasterPos2f(0, gl_height + (0 - self->frequency_list.size()  - 9) * line_height );
  glutBitmapString(font, (unsigned char*) line);  
 
  snprintf(line, 70,  "joint speed %2.0f deg/s", self->current_plan.desired_joint_speed*180/M_PI);
  glColor3f(  0.0, 0.0, 0.8 );
  glRasterPos2f(0, gl_height + (-1 - self->frequency_list.size()  - 9) * line_height);
  glutBitmapString(font, (unsigned char*) line);  
  
  snprintf(line, 70,  "%s", self->current_plan.planner_name.c_str() );
  glColor3f(  0.0, 0.0, 0.8 );
  glRasterPos2f(0, gl_height + (-2 - self->frequency_list.size()  - 9) * line_height);
  glutBitmapString(font, (unsigned char*) line);  

  snprintf(line, 70,  "Duration %2.2fsec",   self->current_plan.plan_execution_time  );
  glColor3f(  0.0, 0.0, 0.8 );
  glRasterPos2f(0, gl_height + (-3 - self->frequency_list.size()  - 9) * line_height);
  glutBitmapString(font, (unsigned char*) line);  
  
  /// Status Block:  
  char line1[80], line2[80], line3[80], line4[80], line5[80], line6[80], line7[90], line8[90], line9[90];
  
  snprintf(line1,70, "est latency %d", self->estimated_latency_ms);
  snprintf(line2,70, "target kbps %.0f", (float) self->target_bps/1000);
  
  
  snprintf(line3,70, " pitch%5.2f hd%5.2f",self->pitch,self->head_pitch);
  snprintf(line4,70, "  roll%5.2f hd%5.2f",self->roll,self->head_roll); 
  snprintf(line5,70, "height%5.2f hd%5.2f",self->height,self->head_height); 

  if ((self->left_foot_contact==1)&& (self->right_foot_contact==1) ){
    snprintf(line6,70, "  feet LR %5.2f", self->foot_spacing);
  }else if(self->left_foot_contact==1){
    snprintf(line6,70, "  feet L* %5.2f", self->foot_spacing);
  }else if(self->right_foot_contact==1){
    snprintf(line6,70, "  feet *R %5.2f", self->foot_spacing);
  }else{
    snprintf(line6,70, "  feet ** %5.2f", self->foot_spacing);
  }   
  snprintf(line7,70, "%06.1f %06.1f",self->l_foot_force_z,self->r_foot_force_z); 

  float elapsed_control_time =  (self->last_utime - self->controller_utime)*1E-6;
  std::string control_status;
  if (self->controller_state == DRC_CONTROLLER_STATUS_T_UNKNOWN){ control_status ="UNKNOWN"; 
  }else if (self->controller_state == DRC_CONTROLLER_STATUS_T_STANDING){ control_status ="STANDING";
  }else if (self->controller_state == DRC_CONTROLLER_STATUS_T_WALKING){ control_status ="WALKING"; 
  }else if (self->controller_state == DRC_CONTROLLER_STATUS_T_HARNESSED){ control_status ="HARNESS"; 
  }else if (self->controller_state == DRC_CONTROLLER_STATUS_T_QUASISTATIC){ control_status ="QSTATIC"; 
  }else if (self->controller_state == DRC_CONTROLLER_STATUS_T_BRACING){ control_status ="BRACING"; 
  }else if (self->controller_state == DRC_CONTROLLER_STATUS_T_CRAWLING){ control_status ="CRAWLING"; 
  }else if (self->controller_state == DRC_CONTROLLER_STATUS_T_DUMMY){ control_status ="DUMMY"; 
  }else{ control_status ="UNSPECIFIED"; // shouldnt happen
  }
  if (fabs(elapsed_control_time) >= 100){
     elapsed_control_time =0;
  }
  snprintf(line8,70, "MIT %s [%.1f]", control_status.c_str() , elapsed_control_time);

  
  float elapsed_atlas_time =  (self->last_utime - self->atlas_utime )*1E-6;
  std::string atlas_status;
  if (self->atlas_state == 0){ atlas_status ="NONE"; 
  }else if (self->atlas_state == 1){ atlas_status ="FREEZE";    
  }else if (self->atlas_state == 2){ atlas_status ="PREP";
  }else if (self->atlas_state == 3){ atlas_status ="STAND";
  }else if (self->atlas_state == 4){ atlas_status ="WALKING"; 
  }else if (self->atlas_state == 5){ atlas_status ="STEP"; 
  }else if (self->atlas_state == 6){ atlas_status ="MANIP"; 
  }else if (self->atlas_state == 7){ atlas_status ="USER"; 
  }else if (self->atlas_state == 8){ atlas_status ="CALIB"; 
  }else{ atlas_status ="UNKNOWN";
  }
  if (fabs(elapsed_atlas_time) >= 100){
     elapsed_atlas_time =0;
  }
  snprintf(line9,70, "BDI %s [%.1f]", atlas_status.c_str() , elapsed_atlas_time);
  
  //snprintf(line9,70, "%.3f", ((double)self->last_utime/1E6) );
  
    
  int x = 0;
  int y = gl_height - 8 * line_height;

  int x_pos = 189; // text lines x_position
  if (self->shading){
    glColor4f(0, 0, 0, 0.7);
    glBegin(GL_QUADS);
    glVertex2f(x, y - line_height);
    glVertex2f(x + 21*9, y - line_height); // 21 is the number of chars in the box
    glVertex2f(x + 21*9, y + 8 * line_height); // 21 is the number of chars in the box
    glVertex2f(x     , y + 8 * line_height);
    glEnd();

    // scrolling text background://////////////////////////////
    glColor4f(0, 0, 0, 0.7);
    glBegin(GL_QUADS);
    glVertex2f(x_pos, y - line_height);
    glVertex2f(gl_width, y - line_height); // 21 is the number of chars in the box
    glVertex2f(gl_width, y + 8 * line_height); // 21 is the number of chars in the box
    glVertex2f(x_pos, y + 8 * line_height);
    glEnd();  
  }

  glColor3fv(safe_colors[0]);
  glRasterPos2f(x, y );
  glutBitmapString(font, (unsigned char*) line1);

  glColor3fv(safe_colors[1]);
  glRasterPos2f(x, y + 1 * line_height );
  glutBitmapString(font, (unsigned char*) line2);
  
  glColor3fv(safe_colors[2]);
  glRasterPos2f(x, y + 2 * line_height );
  glutBitmapString(font, (unsigned char*) line3);
  
  glColor3fv(safe_colors[0]);
  glRasterPos2f(x, y + 3 * line_height);
  glutBitmapString(font, (unsigned char*) line4);
  glColor3fv(safe_colors[1]);
  glRasterPos2f(x, y + 4 * line_height);
  glutBitmapString(font, (unsigned char*) line5);
  glColor3fv(safe_colors[2]);
  glRasterPos2f(x, y + 5 * line_height);
  glutBitmapString(font, (unsigned char*) line6);

  glColor3fv(safe_colors[0]);
  glRasterPos2f(x, y + 6 * line_height);
  glutBitmapString(font, (unsigned char*) line7);
  glColor3fv(safe_colors[1]);
  glRasterPos2f(x, y + 7 * line_height);
  glutBitmapString(font, (unsigned char*) line8);
  glColor3fv(safe_colors[2]);
  glRasterPos2f(x, y + 8 * line_height);
  glutBitmapString(font, (unsigned char*) line9);
  
  
  
  /// scrolling text://////////////////////////////
  int y_pos=0;
  char scroll_line[80];
  int W_to_show=0;
  if (!self->param_status[0]){
    W_to_show=1;
  }
  int N_active_params=0;
  for (int j=0; j < NUMBER_OF_SYSTEMS ; j++){
    if (self->param_status[j]){
    //printf ("%d is active\n",j);
    N_active_params++;
    W_to_show=j;
    }
  }
  
  int max_bottom_strip = 6;//15;
  if (1==1){
  
    if (N_active_params==1){ // if only one class wants to be shown:
    //printf ("show widget number %d ? %d\n",W_to_show,self->param_status[W_to_show]);
    //   printf("refresh\n");
    //glColor3fv(colors[1]);
    int msgs_onscreen = 0;
	// go through the circular list
    for (int i=self->deques[W_to_show]->size()-1 ;i>=0;i--){
     // stop of we have covered the screen:
     if (y_pos > (gl_height)){
	   break;
     }
     y_pos = (int) line_height*msgs_onscreen;
     float colors4[4] = {0};
     colors4[0] =safe_colors[W_to_show][0];
     colors4[1] =safe_colors[W_to_show][1];
     colors4[2] =safe_colors[W_to_show][2];
     colors4[3] =1;
     if (msgs_onscreen >  (max_bottom_strip) ){
      if (self->visability==MODE_FULL){
        colors4[3] = 1;
      }else if (self->visability==MODE_NONE){
        colors4[3] = 0;
      }else{
        colors4[3] = 0.4;
      }
     }
     string temp;
     temp= self->deques[W_to_show]->at(i)->value;
     long long this_utime= self->deques[W_to_show]->at(i)->utime;
     int age_of_msg =(int) (bot_timestamp_now() - this_utime)/1000000;
     snprintf(scroll_line,70, "%5.d %s",age_of_msg,temp.c_str());
     // use this for debugging:
     //snprintf(scroll_line,70, "%5.d %s line number %d - ctr: %d",age_of_msg,temp.c_str(),i,ctr);
     //    glColor3fv(colors[1]);
     glColor4fv(colors4);
     glRasterPos2f(x_pos,  (int)gl_height -y_pos  );
     glutBitmapString(font, (unsigned char*) scroll_line);  
     msgs_onscreen++;
     if (msgs_onscreen > MAXIMUM_N_OF_LINES) {
	   break;
     }
     }
   }else if((N_active_params > 1)&&(self->param_important)){
     //printf("important\n");
     int msgs_onscreen = 0;
     for (int i=self->sys_deque->size()-1 ;i>=0;i--){
     if (y_pos > (gl_height)){ // stop of we have covered the screen:
	   break;
     }
     string temp;
     temp= self->sys_deque->at(i)->value;
     long long this_utime= self->sys_deque->at(i)->utime;
     int age_of_msg =(int) (bot_timestamp_now() - this_utime)/1000000;
     int W_to_show_comb = (int) self->sys_deque->at(i)->system;
     //cout << "i  should choose colour" << W_to_show_comb << endl; 
       if (self->param_status[W_to_show_comb]){
       y_pos = line_height*msgs_onscreen;
       float colors4[4] = {0};
       colors4[0] =safe_colors[W_to_show_comb][0];
       colors4[1] =safe_colors[W_to_show_comb][1];
       colors4[2] =safe_colors[W_to_show_comb][2];
       colors4[3] =1.0;
       if (msgs_onscreen >  (max_bottom_strip) ){
         if (self->visability==MODE_FULL){
         colors4[3] = 1.0;
         }else if (self->visability==MODE_NONE){
         colors4[3] = 0.0;
         }else{
         colors4[3] = 0.4;
         }
       }
       snprintf(scroll_line,70, "%5.d %s",age_of_msg,temp.c_str());
       // use this for debugging:
       //snprintf(scroll_line,70, "%5.d %s line number %d - ctr: %d",age_of_msg,temp.c_str(),i,ctr);
       //    glColor3fv(safe_colors[1]);
       glColor4fv(colors4);
       glRasterPos2f(x_pos, (int) gl_height -y_pos   );
       glutBitmapString(font, (unsigned char*) scroll_line);  
       msgs_onscreen++;
       if (msgs_onscreen > MAXIMUM_N_OF_LINES) {
        break;
       }
     }
     }
   }else if (N_active_params > 1){ // if only two classes want to be shown:
     //      printf ("show multiple widgets\n");
     //   printf("refresh\n");
     //glColor3fv(safe_colors[1]);
     int msgs_onscreen = 0;
     for (int i=self->sys_deque->size()-1 ;i>=0;i--){
     if (y_pos > (gl_height)){ // stop of we have covered the screen:
	   break;
     }
     string temp;
     temp= self->sys_deque->at(i)->value;
     long long this_utime= self->sys_deque->at(i)->utime;
     int age_of_msg =(int) (bot_timestamp_now() - this_utime)/1000000;
     int W_to_show_comb = (int) self->sys_deque->at(i)->system;
     //cout << "i  should choose colour" << W_to_show_comb << endl; 
     if (self->param_status[W_to_show_comb]){
       y_pos = line_height*((float)msgs_onscreen );
       float colors4[4] = {0.0};
       colors4[0] =safe_colors[W_to_show_comb][0];
       colors4[1] =safe_colors[W_to_show_comb][1];
       colors4[2] =safe_colors[W_to_show_comb][2];
       colors4[3] =1;
       if (msgs_onscreen >  (max_bottom_strip) ){
	    if (self->visability==MODE_FULL){
		colors4[3] = 1.0;
	    }else if (self->visability==MODE_NONE){
		colors4[3] = 0.0;
	    }else{
		colors4[3] = 0.4;
	    }
       	
       }
       // string str2 = temp.substr (1,temp.size()-1); // strip the first char off the string
       snprintf(scroll_line,70, "%5.d %s",age_of_msg,temp.c_str());
       // use this for debugging:
       //snprintf(scroll_line,70, "%5.d %s line number %d - ctr: %d",age_of_msg,temp.c_str(),i,ctr);
       //    glColor3fv(safe_colors[1]);
       glColor4fv(colors4);
       glRasterPos2f(x_pos, gl_height -y_pos  );
       glutBitmapString(font, (unsigned char*) scroll_line);  
       msgs_onscreen++;
       if (msgs_onscreen > MAXIMUM_N_OF_LINES) {
	   break;
       }
     }
     }
   }
  }
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glPopAttrib ();
  bot_viewer_request_redraw (self->viewer);
}



static void on_param_widget_changed(BotGtkParamWidget *pw, const char *param, void *user_data) {
  RendererSystemStatus *self = (RendererSystemStatus*) user_data;

  self->param_status[0] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_0);
  self->param_status[1] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_1);
  self->param_status[2] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_2);
  self->param_status[3] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_3);
  self->param_status[4] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_4);
  self->param_status[5] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_5);
  self->param_status[6] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_6);
  self->param_status[7] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_7);
  self->param_status[8] = bot_gtk_param_widget_get_bool(self->pw, PARAM_STATUS_8);
  //fprintf(stderr, "Param Status : %d\n", self->param_status[5]);
  self->param_important = bot_gtk_param_widget_get_bool(self->pw, PARAM_IMPORTANT);
  self->shading = bot_gtk_param_widget_get_bool(self->pw, PARAM_SHADING);
  self->visability = bot_gtk_param_widget_get_enum (self->pw, PARAM_MODE);
  
  bot_viewer_request_redraw (self->viewer);
}

// ------------------------------ Up and Down ------------------------------- //
static void on_load_preferences (BotViewer *viewer, GKeyFile *keyfile, void *user_data) {
  RendererSystemStatus *self = (RendererSystemStatus*)user_data;
  bot_gtk_param_widget_load_from_key_file (self->pw, keyfile, RENDERER_NAME);
}

static void on_save_preferences (BotViewer *viewer, GKeyFile *keyfile, void *user_data){
  RendererSystemStatus *self = (RendererSystemStatus*)user_data;
  bot_gtk_param_widget_save_to_key_file (self->pw, keyfile, RENDERER_NAME);
}

static void _destroy(BotRenderer *r){
  if (!r) return;

  RendererSystemStatus *self = (RendererSystemStatus*)r->user;
  delete self;
}

BotRenderer *renderer_status_new(BotViewer *viewer, int render_priority, lcm_t *lcm){
//  RendererSystemStatus *self = (RendererSystemStatus*)calloc(1, sizeof(RendererSystemStatus));
  
  RendererSystemStatus *self = (RendererSystemStatus*)new (RendererSystemStatus);
  
  
  
  self->lcm = lcm;
  self->viewer = viewer;
  BotRenderer *r = &self->renderer;
  r->user = self;
  self->renderer.name = "System Status";
  self->renderer.draw = _draw;
  self->renderer.destroy = _destroy;

  self->sys_deque = new deque<drc_system_status_t *> ();
//  deque<drc_system_status_t *> * adeque; = new deque<drc_system_status_t *> ();
  for (int i=0;i< NUMBER_OF_SYSTEMS;i++){
    self->deques.push_back( new deque<drc_system_status_t *> () );
  }
  
  self->naffs=0;
  self->pitch = self->head_pitch = self->roll = self->head_roll = 0;
  self->height = self->head_height = self->speed = self->cmd_speed = 0;

  self->left_foot_contact = 0.0;
  self->right_foot_contact = 0.0;
  self->estimated_biases_converged=0.0;
  self->controller_state= DRC_CONTROLLER_STATUS_T_UNKNOWN;
  self->controller_utime= 0;

  self->atlas_state= -1;
  self->atlas_utime= 0;
  self->last_grasp_opt_status_utime = 0;
  self->foot_spacing = 0;
  
  self->right_hand_contact = 0;
  self->left_hand_contact = 0;

  self->l_foot_force_z = 0;
  self->r_foot_force_z = 0;
  
  self->current_plan.desired_joint_speed = 0;
  self->current_plan.desired_ee_arc_speed = 0;
  self->current_plan.planner_name ="Planner & Mode";
  self->current_plan.plan_execution_time = 0;
  
  std::string channel_name ="SAM";
  self->msgchannels.push_back(channel_name);
  channel_name ="CONTROL";
  self->msgchannels.push_back(channel_name);
  channel_name ="RECON";
  self->msgchannels.push_back(channel_name);

  self->status_sub = drc_system_status_t_subscribe(self->lcm, 
      "SYSTEM_STATUS", on_drc_system_status, self);  
  bot_core_pose_t_subscribe(self->lcm,"POSE_BODY",on_pose_body,self);
  bot_core_pose_t_subscribe(self->lcm,"POSE_HEAD",on_pose_head,self);
  bot_core_pose_t_subscribe(self->lcm,"POSE_LEFT_FOOT",on_foot_left,self);
  bot_core_pose_t_subscribe(self->lcm,"POSE_RIGHT_FOOT",on_foot_right,self);
  drc_affordance_collection_t_subscribe(self->lcm,"AFFORDANCE_COLLECTION",on_affordance_collection,self);
  drc_robot_state_t_subscribe(self->lcm,"EST_ROBOT_STATE",on_robot_state,self);
  drc_utime_t_subscribe(self->lcm,"ROBOT_UTIME",on_utime,self);
  drc_frequency_t_subscribe(self->lcm,"FREQUENCY_LCM",on_frequency,self);
  drc_foot_contact_estimate_t_subscribe(self->lcm,"FOOT_CONTACT_ESTIMATE",on_foot_contact,self);
  drc_atlas_status_t_subscribe(self->lcm,"ATLAS_STATUS",on_atlas_status,self);
  
  drc_controller_status_t_subscribe(self->lcm,"CONTROLLER_STATUS",on_controller_status,self);
  drc_estimated_biases_t_subscribe(self->lcm,"ESTIMATED_ACCEL_BIASES",on_estimated_bias,self);
  drc_grasp_opt_status_t_subscribe(self->lcm,"GRASP_OPT_STATUS",on_grasp_opt_status, self);

  drc_hand_tactile_state_t_subscribe(self->lcm,".*_TACTILE_STATE",on_tactile_state, self);
  
  drc_planner_config_t_subscribe(self->lcm,"PLANNER_CONFIG",on_planner_config, self);
  
  drc_bandwidth_stats_t_subscribe(self->lcm,"BASE_BW_STATS",on_bw_stats, self);

  self->param_status[0] = PARAM_STATUS_0_DEFAULT;  
  self->param_status[1] = PARAM_STATUS_1_DEFAULT;  
  self->param_status[2] = PARAM_STATUS_2_DEFAULT;  
  self->param_status[3] = PARAM_STATUS_3_DEFAULT;  
  self->param_status[4] = PARAM_STATUS_4_DEFAULT;  
  self->param_status[5] = PARAM_STATUS_5_DEFAULT;  
  self->param_status[6] = PARAM_STATUS_6_DEFAULT;  
  self->param_important = PARAM_IMPORTANT_DEFAULT;
  
  if (viewer) {
  self->renderer.widget = gtk_alignment_new (0, 0.5, 1.0, 0);

  self->pw = BOT_GTK_PARAM_WIDGET (bot_gtk_param_widget_new ());
  GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
  self->vbox = vbox;
  gtk_container_add (GTK_CONTAINER (self->renderer.widget), vbox);
  gtk_widget_show (vbox);

  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (self->pw), 
            FALSE, TRUE, 0);  
  
   //   gtk_box_pack_start (GTK_BOX (self->renderer.widget), GTK_WIDGET (self->pw), 
   //             FALSE, TRUE, 0);
  bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                      PARAM_STATUS_0, PARAM_STATUS_0_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_1, PARAM_STATUS_1_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_2, PARAM_STATUS_2_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_3, PARAM_STATUS_3_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_4, PARAM_STATUS_4_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_5, PARAM_STATUS_5_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_6, PARAM_STATUS_6_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_7, PARAM_STATUS_7_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_STATUS_8, PARAM_STATUS_8_DEFAULT, NULL);
    

    bot_gtk_param_widget_add_enum (self->pw, PARAM_MODE, (BotGtkParamWidgetUIHint)0, MODE_FADE, 
	      "Full", MODE_FULL, "Fade", MODE_FADE, 
	      "None", MODE_NONE, NULL);    
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_IMPORTANT, PARAM_IMPORTANT_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_SHADING, PARAM_SHADING_DEFAULT, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, (BotGtkParamWidgetUIHint)0,
                                      PARAM_RATES, PARAM_RATES_DEFAULT, NULL);  
    
    
    gtk_widget_show (GTK_WIDGET (self->pw));
        
    g_signal_connect (G_OBJECT (self->pw), "changed",
                      G_CALLBACK (on_param_widget_changed), self);
  
    // save widget modes:
    g_signal_connect (G_OBJECT (viewer), "load-preferences",
      G_CALLBACK (on_load_preferences), self);
    g_signal_connect (G_OBJECT (viewer), "save-preferences",
      G_CALLBACK (on_save_preferences), self);

    /*    // mfallon, save widget modes:
    g_signal_connect (G_OBJECT (viewer), "load-preferences",
                G_CALLBACK (on_load_preferences), self);
    g_signal_connect (G_OBJECT (viewer), "save-preferences",
                G_CALLBACK (on_save_preferences), self);   
    */  
  }

  //    bot_viewer_add_renderer(viewer, &self->renderer, render_priority);
  //    bot_viewer_add_event_handler(viewer, ehandler, render_priority);
  return r;
}

void status_add_renderer_to_viewer(BotViewer *viewer, int render_priority, lcm_t *lcm){
  bot_viewer_add_renderer(viewer, renderer_status_new(viewer,
    render_priority, lcm), render_priority);
}
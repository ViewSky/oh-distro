#include "fovision.hpp"

FoVision::FoVision(boost::shared_ptr<lcm::LCM> &lcm_,
  boost::shared_ptr<fovis::StereoCalibration> kcal):
  lcm_(lcm_), kcal_(kcal), odom_(kcal_->getLeftRectification(),  
  FoVision::getDefaultOptions()), //stereo_depth_(kcal_.get()),
  pose_(Eigen::Isometry3d::Identity())
{
//  fovis::CameraIntrinsicsParameters rgb_params = kcal_->getParameters().rgb_params;
//  depth_image_ = new fovis::DepthImage(rgb_params, rgb_params.width, rgb_params.height);
//  depth_data_ = new float[rgb_params.width * rgb_params.height];

  fovis::VisualOdometryOptions vo_opts = getDefaultOptions();  
  // typical left/right stereo
  stereo_depth_ = new fovis::StereoDepth(kcal_.get(), vo_opts);
  // left/disparity from multisense
// disabledfornow  stereo_disparity_= new fovis::StereoDisparity( kcal_.get()) ;// vo_opts); vo_opts no longer required
  
 // converted_depth_data_= new float[rgb_params.width * rgb_params.height];  
}


/*
FoVision::FoVision(boost::shared_ptr<lcm::LCM> &lcm_):
  lcm_(lcm_), kcal_(default_config()), odom_(kcal_->getLeftRectification(), 
  fovis::VisualOdometry::getDefaultOptions()), //depth_producer_(kcal_.get()), 
  pose_(Eigen::Isometry3d::Identity())
{
  fovis::CameraIntrinsicsParameters rgb_params = kcal_->getParameters().rgb_params;
  depth_image_ = new fovis::DepthImage(rgb_params, rgb_params.width, rgb_params.height);
  depth_data_ = new float[rgb_params.width * rgb_params.height];
} 
*/

FoVision::~FoVision()
{
  delete stereo_depth_;
// disabledfornow  delete stereo_disparity_;
}



// Typical Stereo:
void FoVision::doOdometry(uint8_t *left_buf,uint8_t *right_buf){
  stereo_depth_->setRightImage(right_buf);
  odom_.processFrame(left_buf, stereo_depth_);
  const fovis::MotionEstimator * me = odom_.getMotionEstimator();
  printf("Inliers: %4d  Rep. fail: %4d Matches: %4d Feats: %4d Mean err: %5.2f\n",
          me->getNumInliers(),
          me->getNumReprojectionFailures(),
          me->getNumMatches(),
          (int) odom_.getTargetFrame()->getNumKeypoints(),
          me->getMeanInlierReprojectionError());
}

// Left and Disparity:
void FoVision::doOdometry(uint8_t *left_buf,float *disparity_buf){
// disabledfornow
/*  stereo_disparity_->setDisparityData(disparity_buf);
  odom_.processFrame(left_buf, stereo_disparity_);
  const fovis::MotionEstimator * me = odom_.getMotionEstimator();
  printf("Inliers: %4d  Rep. fail: %4d Matches: %4d Feats: %4d Mean err: %5.2f\n",
          me->getNumInliers(),
          me->getNumReprojectionFailures(),
          me->getNumMatches(),
          (int) odom_.getTargetFrame()->getNumKeypoints(),
          me->getMeanInlierReprojectionError());
*/
}

void FoVision::fovis_stats(){
  

  
  
  
  Eigen::Isometry3d cam_to_local = odom_.getPose();
  
  // rotate coordinate frame so that look vector is +X, and up is +Z
  Eigen::Matrix3d M;
  M <<  0,  0, 1,
       -1,  0, 0,
        0, -1, 0;

  cam_to_local = M * cam_to_local;
  Eigen::Vector3d translation(cam_to_local.translation());
  Eigen::Quaterniond rotation(cam_to_local.rotation());
  rotation = rotation * M.transpose();  
  
  Eigen::Isometry3d motion_estimate = odom_.getMotionEstimate();
  fovis_update_t update_msg;
  update_msg.timestamp =  current_timestamp_;// msg->timestamp;//secs * 1E6 + nsecs/1E3;
  update_msg.prev_timestamp = prev_timestamp_;
  Eigen::Vector3d motion_T = motion_estimate.translation();
  update_msg.translation[0] = motion_T(0);
  update_msg.translation[1] = motion_T(1);
  update_msg.translation[2] = motion_T(2);
  Eigen::Quaterniond motion_R = Eigen::Quaterniond(motion_estimate.rotation());
  update_msg.rotation[0] = motion_R.w();
  update_msg.rotation[1] = motion_R.x();
  update_msg.rotation[2] = motion_R.y();
  update_msg.rotation[3] = motion_R.z();
  
  //get the cov
  const Eigen::MatrixXd & motion_cov = odom_.getMotionEstimateCov();
  for (int i=0;i<6;i++)
    for (int j=0;j<6;j++)
      update_msg.covariance[i][j] =motion_cov(i,j);
    
    
  const fovis::MotionEstimator* me = odom_.getMotionEstimator();
  fovis::MotionEstimateStatusCode estim_status = odom_.getMotionEstimateStatus();
  
  // TODO: set this in the constructor:
  bool verbose=true;
  
  switch(estim_status) {
    case fovis::NO_DATA:
      break;
    case fovis::SUCCESS:
      update_msg.estimate_status = FOVIS_UPDATE_T_ESTIMATE_VALID;
      if (verbose){
	printf("Inliers: %4d  Rep. fail: %4d Matches: %4d Feats: %4d Mean err: %5.2f\n",
          me->getNumInliers(),
          me->getNumReprojectionFailures(),
          me->getNumMatches(),
          (int) odom_.getTargetFrame()->getNumKeypoints(),
          me->getMeanInlierReprojectionError());
      }
      break;
    case fovis::INSUFFICIENT_INLIERS:
      update_msg.estimate_status = FOVIS_UPDATE_T_ESTIMATE_INSUFFICIENT_FEATURES;
      if (verbose){
	printf("Insufficient inliers\n");
      }
      break;
    case fovis::OPTIMIZATION_FAILURE:
      update_msg.estimate_status = FOVIS_UPDATE_T_ESTIMATE_DEGENERATE;
      if (verbose){
	printf("Unable to solve for rigid body transform\n");
      }
      break;
    case fovis::REPROJECTION_ERROR:
      update_msg.estimate_status = FOVIS_UPDATE_T_ESTIMATE_REPROJECTION_ERROR;
      if (verbose){
	printf("Excessive reprojection error (%f).\n", me->getMeanInlierReprojectionError());
      }
      break;
    default:
      if (verbose){
      printf("Unknown error (this should never happen)\n");
      }
      break;
  }
  
  if (estim_status !=  fovis::NO_DATA) {
    fovis_update_t_publish(lcm_->getUnderlyingLCM(), 
                "KINECT_REL_ODOMETRY_FOVISION", &update_msg);
  }

  bool publish_fovis_stats=1;
  if (estim_status !=  fovis::NO_DATA && publish_fovis_stats) {
    fovis_stats_t stats_msg;
    stats_msg.timestamp = update_msg.timestamp;
    stats_msg.num_matches = me->getNumMatches();
    stats_msg.num_inliers = me->getNumInliers();
    stats_msg.mean_reprojection_error = me->getMeanInlierReprojectionError();
    stats_msg.num_reprojection_failures = me->getNumReprojectionFailures();
    const fovis::OdometryFrame * tf(odom_.getTargetFrame());
    stats_msg.num_detected_keypoints = tf->getNumDetectedKeypoints();
    stats_msg.num_keypoints = tf->getNumKeypoints();
    stats_msg.fast_threshold = odom_.getFastThreshold();
    fovis_stats_t_publish(lcm_->getUnderlyingLCM(), "FOVIS_STATS", &stats_msg);
  }  
  
  // publish current pose
  bool publish_pose=0;
  if (publish_pose) {
      bot_core_pose_t pose_msg;
      memset(&pose_msg, 0, sizeof(pose_msg));
      pose_msg.utime =   0;// msg->timestamp;
      pose_msg.pos[0] = translation[0];
      pose_msg.pos[1] = translation[1];
      pose_msg.pos[2] = translation[2];
      pose_msg.orientation[0] = rotation.w();
      pose_msg.orientation[1] = rotation.x();
      pose_msg.orientation[2] = rotation.y();
      pose_msg.orientation[3] = rotation.z();
      bot_core_pose_t_publish(lcm_->getUnderlyingLCM(), "POSE", &pose_msg);
      //  printf("[%6.2f %6.2f %6.2f]\n", translation[0], translation[1], translation[2]);
  }  
  
  bool publish_frame_update=0;
  if (publish_frame_update) {
    //publish the frame update message as well
    bot_core_rigid_transform_t iso_msg;
    iso_msg.utime =0;/// msg->timestamp;
    for (int i = 0; i < 3; i++)
      iso_msg.trans[i] = translation[i];
    iso_msg.quat[0] = rotation.w();
    iso_msg.quat[1] = rotation.x();
    iso_msg.quat[2] = rotation.y();
    iso_msg.quat[3] = rotation.z();
    bot_core_rigid_transform_t_publish(lcm_->getUnderlyingLCM(),
               "BODY_TO_LOCAL", &iso_msg);
  }
  
  
  bool print_translation=0;
  if (print_translation){
    pose_ = pose_ * motion_estimate;
    cam_to_local = pose_;

    // rotate coordinate frame so that look vector is +X, and down is +Z
    //Eigen::Matrix3d M;
    //M <<  0,  0, 1,
    //      1,  0, 0,
    //      0,  1, 0;

    Eigen::Vector3d rpy = (cam_to_local.rotation()*M.transpose()).eulerAngles(0, 1, 2);
    std::cout << "xyz-rpy: " << translation(0) << " " << translation(1) << " " << translation(2) << " -- "
                              << rpy(0)/M_PI*180.0 << " " << rpy(1)/M_PI*180.0 << " " << rpy(2)/M_PI*180.0 << " " << std::endl;
  }
  
  prev_timestamp_ = update_msg.timestamp;

  // leaving this in but it has noeffect:
  //if (estim_status != fovis::SUCCESS) return;
  //if (std::isnan(motion_estimate.translation().x())) return;
  
  
}


fovis::StereoCalibration* FoVision::default_config(){
  // make up an initial calibration
  // This is calibration information for a specific Kinect
  fovis::StereoCalibrationParameters kparams;

  fovis::CameraIntrinsicsParameters lparams;
  lparams.width = 1024;
  lparams.height= 1088;
  lparams.fx = 606.0344848632812;
  lparams.fy = 606.0344848632812;
  lparams.cx = 512.0; // should this have another 0.5?
  lparams.cy = 544.0; // should this have another 0.5?
  lparams.k1 = 0.0;
  lparams.k2 = 0.0;
  lparams.k3 = 0.0;
  lparams.p1 = 0.0;
  lparams.p2 = 0.0;

  fovis::CameraIntrinsicsParameters rparams;
  rparams.width = 1024;
  rparams.height = 1088;
  rparams.fx = 606.0344848632812;
  rparams.fy = 606.0344848632812;
  rparams.cx = 512.0; // should this have another 0.5?
  rparams.cy = 544.0; // should this have another 0.5?
  rparams.k1 = 0.0;
  rparams.k2 = 0.0;
  rparams.k3 = 0.0;
  rparams.p1 = 0.0;
  rparams.p2 = 0.0;

  kparams.left_parameters = lparams;
  kparams.right_parameters = rparams;

  kparams.right_to_left_translation[0] = -0.07;
  kparams.right_to_left_translation[1] =  0.0;
  kparams.right_to_left_translation[2] =  0.0;
  kparams.right_to_left_rotation[0] = 1.0;
  kparams.right_to_left_rotation[1] = 0.0;
  kparams.right_to_left_rotation[2] = 0.0;
  kparams.right_to_left_rotation[3] = 0.0;

/*
  kparams.width = 640;
  kparams.height = 480;

  kparams.depth_params.width = kparams.width;
  kparams.depth_params.height = kparams.height;
  kparams.depth_params.fx = 576.09757860;
  kparams.depth_params.fy = kparams.depth_params.fx;
  kparams.depth_params.cx = 321.06398107;
  kparams.depth_params.cy = 242.97676897;

  kparams.rgb_params.width = kparams.width;
  kparams.rgb_params.height = kparams.height;
  kparams.rgb_params.fx = 576.09757860;
  kparams.rgb_params.fy = kparams.rgb_params.fx;
  kparams.rgb_params.cx = 321.06398107;
  kparams.rgb_params.cy = 242.97676897;

  kparams.shift_offset = 1079.4753;
  kparams.projector_depth_baseline = 0.07214;

  Eigen::Matrix3d R;
  R << 0.999999, -0.000796, 0.001256, 0.000739, 0.998970, 0.045368, -0.001291, -0.045367, 0.998970;
  kparams.depth_to_rgb_translation[0] = -0.015756;
  kparams.depth_to_rgb_translation[1] = -0.000923;
  kparams.depth_to_rgb_translation[2] =  0.002316;
  Eigen::Quaterniond Q(R);
  kparams.depth_to_rgb_quaternion[0] = Q.w();
  kparams.depth_to_rgb_quaternion[1] = Q.x();
  kparams.depth_to_rgb_quaternion[2] = Q.y();
  kparams.depth_to_rgb_quaternion[3] = Q.z();
*/

  return new fovis::StereoCalibration(kparams);
}




fovis::VisualOdometryOptions FoVision::getDefaultOptions()
{
  fovis::VisualOdometryOptions vo_opts = fovis::VisualOdometry::getDefaultOptions();

  bool use_hordur_setting =false;
  if(use_hordur_setting){
  // change to stereo 'defaults'
  vo_opts["use-adaptive-threshold"] = "false"; // hordur: use now not very useful - adds noisy features
  vo_opts["fast-threshold"] = "15";
  // hordur: use not and set fast-threshold as 10-15

  // options if uat is true
  vo_opts["feature-window-size"] = "9";
  vo_opts["max-pyramid-level"] = "3";
  vo_opts["min-pyramid-level"] = "0";
  vo_opts["target-pixels-per-feature"] = "250"; 
  //width*height/250 = target number of features for fast detector
  // - related to fast-threshold-adaptive-gain
  // 640x480 pr2 ---> 307200/tppf = nfeatures = 400 (typically for pr2)
  // 1024x620 (1088)> 634880/tppf = nfeatures
  vo_opts["fast-threshold-adaptive-gain"] = "0.002";
  vo_opts["use-homography-initialization"] = "true";
  vo_opts["ref-frame-change-threshold"] = "100"; // hordur: lowering this is a good idea. down to 100 is good. results in tracking to poses much further appart


  // OdometryFrame
  vo_opts["use-bucketing"] = "true"; // dependent on resolution: bucketing of features. might want to increase this...
  vo_opts["bucket-width"] = "50";
  vo_opts["bucket-height"] = "50";
  vo_opts["max-keypoints-per-bucket"] = "10";
  vo_opts["use-image-normalization"] = "true"; //hordur: not of major importance, can turn off, extra computation

  // MotionEstimator
  vo_opts["inlier-max-reprojection-error"] = "1.0"; // putting this down to 1.0 is good - give better alignment
  vo_opts["clique-inlier-threshold"] = "0.1";
  vo_opts["min-features-for-estimate"] = "10";
  vo_opts["max-mean-reprojection-error"] = "8.0";
  vo_opts["use-subpixel-refinement"] = "true"; // hordur: v.important to use
  vo_opts["feature-search-window"] = "25"; // for rapid motion this should be higher - size of area to search for new features
  vo_opts["update-target-features-with-refined"] = "false";

  // StereoDepth
  vo_opts["stereo-require-mutual-match"] = "true";
  vo_opts["stereo-max-dist-epipolar-line"] = "2.0";
  vo_opts["stereo-max-refinement-displacement"] = "2.0";
  vo_opts["stereo-max-disparity"] = "128";

  }else{
// Original:

  // change to stereo 'defaults'
  vo_opts["use-adaptive-threshold"] = "true"; // hordur: use now not very useful - adds noisy features
  vo_opts["fast-threshold"] = "10";
  // hordur: use not and set fast-threshold as 10-15

  // options if uat is true
  vo_opts["feature-window-size"] = "9";
  vo_opts["max-pyramid-level"] = "3";
  vo_opts["min-pyramid-level"] = "0";
  vo_opts["target-pixels-per-feature"] = "250"; 
  //width*height/250 = target number of features for fast detector
  // - related to fast-threshold-adaptive-gain
  // 640x480 pr2 ---> 307200/tppf = nfeatures = 400 (typically for pr2)
  // 1024x620 (1088)> 634880/tppf = nfeatures
  vo_opts["fast-threshold-adaptive-gain"] = "0.002";
  vo_opts["use-homography-initialization"] = "true";
  vo_opts["ref-frame-change-threshold"] = "150"; // hordur: lowering this is a good idea. down to 100 is good. results in tracking to poses much further appart


  // OdometryFrame
  vo_opts["use-bucketing"] = "true"; // dependent on resolution: bucketing of features. might want to increase this...
  vo_opts["bucket-width"] = "50";
  vo_opts["bucket-height"] = "50";
  vo_opts["max-keypoints-per-bucket"] = "10";
  vo_opts["use-image-normalization"] = "true"; //hordur: not of major importance, can turn off, extra computation

  // MotionEstimator
  vo_opts["inlier-max-reprojection-error"] = "2.0"; // putting this down to 1.0 is good - give better alignment
  vo_opts["clique-inlier-threshold"] = "0.1";
  vo_opts["min-features-for-estimate"] = "10";
  vo_opts["max-mean-reprojection-error"] = "8.0";
  vo_opts["use-subpixel-refinement"] = "true"; // hordur: v.important to use
  vo_opts["feature-search-window"] = "25"; // for rapid motion this should be higher - size of area to search for new features
  vo_opts["update-target-features-with-refined"] = "false";

  // StereoDepth
  vo_opts["stereo-require-mutual-match"] = "true";
  vo_opts["stereo-max-dist-epipolar-line"] = "2.0";
  vo_opts["stereo-max-refinement-displacement"] = "2.0";
  vo_opts["stereo-max-disparity"] = "128";
  }

  return vo_opts;
}

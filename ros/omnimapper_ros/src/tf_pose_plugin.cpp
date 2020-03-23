#include <omnimapper_ros/tf_pose_plugin.h>

namespace omnimapper {

TFPosePlugin::TFPosePlugin(omnimapper::OmniMapperBase* mapper)
    : mapper_(mapper),
      nh_("~"),
      tf_listener_(ros::Duration(30.0)),
      odom_frame_name_("/odom"),
      base_frame_name_("/camera_depth_optical_frame"),
      rotation_noise_(1.0),
      translation_noise_(1.0) {}

gtsam::BetweenFactor<gtsam::Pose3>::shared_ptr TFPosePlugin::addRelativePose(
    boost::posix_time::ptime t1, gtsam::Symbol sym1,
    boost::posix_time::ptime t2, gtsam::Symbol sym2) {
  // Convert the timestamps
  ros::Time rt1 = ptime2rostime(t1);
  ros::Time rt2 = ptime2rostime(t2);

  // Get the poses
  tf::StampedTransform tf1;
  tf::StampedTransform tf2;

  // Add the factor
  bool got_tf = true;
  gtsam::Pose3 relative_pose = gtsam::Pose3::identity();
  try {
    tf_listener_.waitForTransform(odom_frame_name_, base_frame_name_, rt1,
                                  ros::Duration(0.2));

    tf_listener_.lookupTransform(odom_frame_name_, base_frame_name_, rt1, tf1);

    tf_listener_.waitForTransform(odom_frame_name_, base_frame_name_, rt2,
                                  ros::Duration(0.2));

    tf_listener_.lookupTransform(odom_frame_name_, base_frame_name_, rt2, tf2);
  } catch (tf::TransformException ex) {
    ROS_INFO(
        "OmniMapper reports :: Transform from %s to %s not yet available.  "
        "Exception: %s",
        odom_frame_name_.c_str(), base_frame_name_.c_str(), ex.what());
    ROS_INFO("Writing identity instead\n");
    got_tf = false;
  }

  if (got_tf) {
    gtsam::Pose3 pose1 = tf2pose3(tf1);
    gtsam::Pose3 pose2 = tf2pose3(tf2);
    // gtsam::Pose3 relative_pose = pose1.between (pose2);
    relative_pose = pose1.between(pose2);
  }

  printf("TFPosePlugin: Adding factor between %zu and %zu\n", sym1.index(),
         sym2.index());
  printf("TFPosePlugin: Relative transform: %lf %lf %lf\n", relative_pose.x(),
         relative_pose.y(), relative_pose.z());

  double trans_noise = translation_noise_;  // 1.0;
  double rot_noise = rotation_noise_;       // 1.0;
  // double roll_noise = 0.01;
  // doulbe pitch_noise = 0.01;
  // gtsam::SharedDiagonal noise = gtsam::noiseModel::Diagonal::Sigmas
  // (gtsam::Vector_ (6, rot_noise, rot_noise, rot_noise, trans_noise,
  // trans_noise, trans_noise));
  gtsam::SharedDiagonal noise = gtsam::noiseModel::Diagonal::Sigmas(
      (gtsam::Vector(6) << roll_noise_, pitch_noise_, yaw_noise_, trans_noise,
       trans_noise, trans_noise));
  // omnimapper::OmniMapperBase::NonlinearFactorPtr between (new
  // gtsam::BetweenFactor<gtsam::Pose3> (sym2, sym1, relative_pose, noise));
  gtsam::BetweenFactor<gtsam::Pose3>::shared_ptr between(
      new gtsam::BetweenFactor<gtsam::Pose3>(sym1, sym2, relative_pose, noise));
  between->print("TF BetweenFactor:\n");
  // mapper_->addFactor (between);
  return (between);
}

bool TFPosePlugin::ready() { return (true); }

}  // namespace omnimapper

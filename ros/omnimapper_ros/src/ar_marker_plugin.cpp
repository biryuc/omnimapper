#include <omnimapper_ros/ar_marker_plugin.h>
#include <omnimapper_ros/ros_time_utils.h>
#include <geometry_msgs/Pose.h>
#include <omnimapper/landmark_factor.h>

namespace omnimapper
{
  ARMarkerPlugin::ARMarkerPlugin (omnimapper::OmniMapperBase* mapper)
    : mapper_ (mapper),
      nh_ ("~")
  {
    marker_sub_ = nh_.subscribe ("/ar_pose_marker", 1, &ARMarkerPlugin::markerCallback, this);
  }
  
  void 
  ARMarkerPlugin::markerCallback (const ar_track_alvar_msgs::AlvarMarkers& msg)
  {
    ROS_INFO ("Got some markers: %d\n", msg.markers.size ());
    
    //omnimapper::Time msg_time = omnimapper::rostime2ptime (msg.header.stamp);
    // HACK since at_track_alvar does not fill in header stamp
    omnimapper::Time msg_time = omnimapper::rostime2ptime (ros::Time::now ());
    
    gtsam::Symbol pose_symbol;
    mapper_->getPoseSymbolAtTime (msg_time, pose_symbol);

    for (int i = 0; i < msg.markers.size (); i++)
    {
      gtsam::Symbol marker_symbol ('a', msg.markers[i].id);
      gtsam::Pose3 relative_pose (gtsam::Rot3::quaternion (msg.markers[i].pose.pose.orientation.w,
                                                           msg.markers[i].pose.pose.orientation.x,
                                                           msg.markers[i].pose.pose.orientation.y,
                                                           msg.markers[i].pose.pose.orientation.z),
                                  gtsam::Point3 (msg.markers[i].pose.pose.position.x,
                                                 msg.markers[i].pose.pose.position.y,
                                                 msg.markers[i].pose.pose.position.z));

      if (known_markers_.count (msg.markers[i].id) == 0)
      {
        ROS_INFO ("AR Plugin: New marker %d observed!\n", msg.markers[i].id);
        mapper_->addNewValue (marker_symbol, relative_pose);
        known_markers_.insert (msg.markers[i].id);
      }
      
      gtsam::SharedDiagonal measurement_noise;
      measurement_noise = gtsam::noiseModel::Diagonal::Sigmas (gtsam::Vector_ (6, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1));

      omnimapper::OmniMapperBase::NonlinearFactorPtr marker_factor (new gtsam::BetweenFactor<gtsam::Pose3> (pose_symbol, marker_symbol, relative_pose, measurement_noise));
      mapper_->addFactor (marker_factor);
    }
    
  }
  
}
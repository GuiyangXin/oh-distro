// Selective ros2lcm translator for Edinburgh Schunk SDH gripper/hand
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <ros/ros.h>
#include <ros/console.h>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <map>

#include <Eigen/Dense>

#include <sensor_msgs/JointState.h>

#include <lcm/lcm-cpp.hpp>
#include "lcmtypes/drc/joint_state_t.hpp"

using namespace std;

class App{
public:
  App(ros::NodeHandle node_);
  ~App();

private:
  lcm::LCM lcm_publish_ ;
  ros::NodeHandle node_;

  ros::Subscriber  joint_states_sub_;
  void joint_states_cb(const sensor_msgs::JointStateConstPtr& msg);

};

App::App(ros::NodeHandle node_) :
    node_(node_){
  ROS_INFO("Initializing Schunk SDH Gripper Translator");
  if(!lcm_publish_.good()){
    std::cerr <<"ERROR: lcm is not good()" <<std::endl;
  }

  joint_states_sub_ = node_.subscribe(string("/gripper/sdh_controller/joint_states"), 100, &App::joint_states_cb,this);

};

App::~App()  {
}


void App::joint_states_cb(const sensor_msgs::JointStateConstPtr& msg){
  int n_joints = msg->position.size();

  if (n_joints == 1)
    return; // The driver sends two messages, one consisting of a single message that will kill the program if not caught - it's a mirrored version of the sdh_knuckle_joint named sdh_joint_21_state

  drc::joint_state_t msg_out;
  msg_out.utime = (int64_t) msg->header.stamp.toNSec()/1000; // from nsec to usec
  
  msg_out.joint_position.assign(n_joints + 1, 0  );
  msg_out.joint_velocity.assign(n_joints + 1, 0  );
  msg_out.joint_effort.assign(n_joints + 1, 0  );
  msg_out.num_joints = n_joints + 1;

  msg_out.joint_name = {"sdh_knuckle_joint", "sdh_thumb_2_joint", "sdh_thumb_3_joint", "sdh_finger_12_joint", "sdh_finger_13_joint", "sdh_finger_22_joint", "sdh_finger_23_joint", "sdh_finger_21_joint"};
  for (int i = 0; i < n_joints; i++)  {
    msg_out.joint_name[ i ] = msg->name[ i ];
    msg_out.joint_position[ i ] = msg->position[ i ];
    msg_out.joint_velocity[ i ] = msg->velocity[ i ];
    msg_out.joint_effort[ i ] = msg->effort[i];
  }
  msg_out.joint_name[ n_joints ] = "sdh_finger_21_joint";
  msg_out.joint_position[ n_joints ] = msg->position[0];
  msg_out.joint_velocity[ n_joints ] = msg->velocity[0];
  msg_out.joint_effort[ n_joints ] = msg->effort[0];

  lcm_publish_.publish("SCHUNK_STATE", &msg_out);
}


int main(int argc, char **argv){
  ros::init(argc, argv, "ros2lcm_sdh");
  ros::NodeHandle nh;
  new App(nh);
  std::cout << "ros2lcm_sdh translator ready\n";
  ROS_ERROR("ROS2LCM Schunk SDH Joint State Translator Ready");
  ros::spin();
  return 0;
}

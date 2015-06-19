// Selective ros2lcm translator for Edinburgh Kuka Arm
// mfallon
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
#include <ipab_msgs/PlannerResponse.h>

#include <lcm/lcm-cpp.hpp>
#include "lcmtypes/drc/joint_state_t.hpp"
#include "lcmtypes/drc/robot_plan_w_keyframes_t.hpp"

using namespace std;

class App{
public:
  App(ros::NodeHandle node_);
  ~App();

private:
  lcm::LCM lcm_publish_ ;
  ros::NodeHandle node_;

  ros::Subscriber  traj_sub_;
  ros::Subscriber  ik_sub_;
  void traj_cb(const ipab_msgs::PlannerResponseConstPtr& msg);
  void ik_cb(const ipab_msgs::PlannerResponseConstPtr& msg);
};

App::App(ros::NodeHandle node_) : node_(node_)
{
  ROS_INFO_STREAM("Initializing KUKA LWR Joint State Translator");
  if(!lcm_publish_.good())
  {
      ROS_ERROR_STREAM("lcm is not good()");
  }

  traj_sub_ = node_.subscribe(string("/exotica/robot_plan"), 100, &App::traj_cb,this);
  ik_sub_ = node_.subscribe(string("/exotica/robot_ik"), 100, &App::ik_cb,this);

}

App::~App()  {
}


void App::traj_cb(const ipab_msgs::PlannerResponseConstPtr& msg)
{
    drc::robot_plan_w_keyframes_t msg_out;
    msg_out.utime = (int64_t) msg->header.stamp.toNSec()/1000; // from nsec to usec
    msg_out.robot_name = msg->robot;
    int T = msg->points.size();
    msg_out.num_states = T;
    msg_out.num_keyframes = 0;
    msg_out.num_breakpoints = 0;
    msg_out.is_keyframe.assign(T,false);
    msg_out.is_breakpoint.assign(T,false);
    msg_out.plan_info.assign(T,msg->info);
    msg_out.num_grasp_transitions = 0;
    msg_out.num_bytes = 0;
    msg_out.plan.resize(T);
    for(int t=0;t<T;t++)
    {
        msg_out.plan[t].utime = msg->points[t].time_from_start.toNSec()/1000;
        int n_joints = msg->joint_names.size();
        msg_out.plan[t].joint_position.assign(n_joints , 0  );
        msg_out.plan[t].joint_velocity.assign(n_joints , 0  );
        msg_out.plan[t].joint_effort.assign(n_joints , 0  );
        msg_out.plan[t].pose.rotation.w=1.0;
        msg_out.plan[t].num_joints = n_joints;
        msg_out.plan[t].joint_name= msg->joint_names;
        for (int i = 0; i < n_joints; i++)
        {
          msg_out.plan[t].joint_position[ i ] = msg->points[t].positions[ i ];
          // Ignoring velocity and effort for now
        }
    }


    lcm_publish_.publish("CANDIDATE_MANIP_PLAN", &msg_out);
}

void App::ik_cb(const ipab_msgs::PlannerResponseConstPtr& msg)
{
    drc::robot_plan_w_keyframes_t msg_out;
    msg_out.utime = (int64_t) msg->header.stamp.toNSec()/1000; // from nsec to usec
    msg_out.robot_name = msg->robot;
    int T = msg->points.size();
    msg_out.num_states = T;
    msg_out.num_keyframes = 0;
    msg_out.num_breakpoints = 0;
    msg_out.is_keyframe.assign(T,false);
    msg_out.is_breakpoint.assign(T,false);
    msg_out.plan_info.assign(T,msg->info);
    msg_out.num_grasp_transitions = 0;
    msg_out.num_bytes = 0;
    msg_out.plan.resize(T);
    for(int t=0;t<T;t++)
    {
        msg_out.plan[t].utime = msg->points[t].time_from_start.toNSec()/1000;
        int n_joints = msg->joint_names.size();
        msg_out.plan[t].joint_position.assign(n_joints , 0  );
        msg_out.plan[t].joint_velocity.assign(n_joints , 0  );
        msg_out.plan[t].joint_effort.assign(n_joints , 0  );
        msg_out.plan[t].pose.rotation.w=1.0;
        msg_out.plan[t].num_joints = n_joints;
        msg_out.plan[t].joint_name= msg->joint_names;
        for (int i = 0; i < n_joints; i++)
        {
          msg_out.plan[t].joint_position[ i ] = msg->points[t].positions[ i ];
          // Ignoring velocity and effort for now
        }
    }


    lcm_publish_.publish("CANDIDATE_MANIP_IKPLAN", &msg_out);
}

int main(int argc, char **argv){
  ros::init(argc, argv, "ros2lcm");
  ros::NodeHandle nh;
  new App(nh);
  ROS_INFO_STREAM("ros2lcm KUKA lwr translator ready");
  ROS_ERROR_STREAM("ROS2LCM KUKA LWR Joint State Translator Ready");
  ros::spin();
  return 0;
}

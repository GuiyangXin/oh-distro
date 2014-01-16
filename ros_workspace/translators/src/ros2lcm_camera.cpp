#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <ros/ros.h>
#include <ros/console.h>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>
#include <iostream>

#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <image_transport/image_transport.h>
#include <image_transport/subscriber_filter.h>
#include <image_geometry/stereo_camera_model.h>

#include <sensor_msgs/image_encodings.h>
#include <cv_bridge/cv_bridge.h>

#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>

#include <lcm/lcm-cpp.hpp>
#include <lcmtypes/bot_core.hpp>
#include <lcmtypes/drc_lcmtypes.hpp>
#include <ros/callback_queue.h>

#include <image_io_utils/image_io_utils.hpp> // to simplify jpeg/zlib compression and decompression

using namespace std;

int width =800;//1024; // hardcoded
int height =800;//544; // hardcoded
uint8_t* stereo_data = new uint8_t [2*3* width*height]; // 2 color scale images stacked
uint8_t* singleimage_data = new uint8_t [2*2* width*height]; // 1 color scale image
int jpeg_quality=90;

class App{
public:
  App(ros::NodeHandle node_);
  ~App();

private:
  lcm::LCM lcm_publish_ ;
  ros::NodeHandle node_;
  void left_image_cb(const sensor_msgs::ImageConstPtr& msg);
  void l_hand_l_image_cb(const sensor_msgs::ImageConstPtr& msg);
  void r_hand_l_image_cb(const sensor_msgs::ImageConstPtr& msg);
  void send_image(const sensor_msgs::ImageConstPtr& msg,string channel );
    
  image_transport::Subscriber left_image_sub_;
  image_transport::ImageTransport it_;
  
  image_transport::Subscriber l_hand_l_image_sub_, r_hand_l_image_sub_;
  
  // Image Compression
  image_io_utils*  imgutils_;  
};

App::App(ros::NodeHandle node_) :it_(node_), node_(node_){
  ROS_INFO("Initializing Translator");
  if(!lcm_publish_.good()){
    std::cerr <<"ERROR: lcm is not good()" <<std::endl;
  }

  // Mono-Cameras:
  left_image_sub_ = it_.subscribe("/multisense_sl/camera/left/image_rect_color", 1, &App::left_image_cb,this);
  //left_image_sub_ = node_.subscribe( "/multisense_sl/camera/left/image_raw", 10, &App::left_image_cb,this);
 
  l_hand_l_image_sub_ = it_.subscribe("/sandia_hands/l_hand/camera/left/image_raw", 1, &App::l_hand_l_image_cb,this);
  r_hand_l_image_sub_ = it_.subscribe("/sandia_hands/r_hand/camera/left/image_raw", 1, &App::r_hand_l_image_cb,this);
  
  
    
  imgutils_ = new image_io_utils( lcm_publish_.getUnderlyingLCM(), width, height );  
};

App::~App()  {
}

int h_counter =0;
void App::left_image_cb(const sensor_msgs::ImageConstPtr& msg){
  if (h_counter%30 ==0){
    ROS_ERROR("LEFTC [%d]", h_counter );
    ///std::cout << h_counter << " head left image\n";
  }  
  h_counter++;
  send_image(msg, "CAMERA_LEFT");
}

int l_counter =0;
void App::l_hand_l_image_cb(const sensor_msgs::ImageConstPtr& msg){
  if (l_counter%30 ==0){
    ROS_ERROR("L H C [%d]", l_counter );
    ///std::cout << l_counter << " left hand image\n";
  }  
  l_counter++;
  send_image(msg, "CAMERALHAND_LEFT");
}

int r_counter =0;
void App::r_hand_l_image_cb(const sensor_msgs::ImageConstPtr& msg){
  if (r_counter%30 ==0){
    ROS_ERROR("R H C [%d]", r_counter );
    ///std::cout << r_counter << " right hand image\n";
  }  
  r_counter++;
  send_image(msg, "CAMERARHAND_LEFT");
}

void App::send_image(const sensor_msgs::ImageConstPtr& msg,string channel ){
  int64_t current_utime = (int64_t) floor(msg->header.stamp.toNSec()/1000);
  /*cout << msg->width << " " << msg->height << " | "
       << msg->encoding << " is encoding | "
       << current_utime << " | "<< channel << "\n";*/

  int n_colors=0;
  if (msg->encoding.compare("mono8") == 0){
    copy(msg->data.begin(), msg->data.end(), singleimage_data);
    n_colors=1;
  }else if (msg->encoding.compare("rgb8") == 0){
    copy(msg->data.begin(), msg->data.end(), singleimage_data);
    n_colors=3;
  }else {
    cout << msg->encoding << " image encoded not supported, not publishing\n";
    cout << channel << "\n";
    return;
  }  
  
  
  int isize = msg->width*msg->height;
  //ROS_ERROR("Received size [%d]", msg->data.size() );
  
  if (1==1){
    imgutils_->jpegImageThenSend(singleimage_data, current_utime, 
                msg->width, msg->height, jpeg_quality, channel, n_colors );
  }else{
    bot_core::image_t lcm_img;
    lcm_img.utime =current_utime;
    lcm_img.width =msg->width;
    lcm_img.height =msg->height;
    lcm_img.nmetadata =0;
    lcm_img.row_stride=n_colors*msg->width;
    if (n_colors ==1){
      lcm_img.pixelformat =bot_core::image_t::PIXEL_FORMAT_GRAY;
      lcm_img.size =2*isize;
    }else if(n_colors ==3){
      lcm_img.pixelformat =bot_core::image_t::PIXEL_FORMAT_RGB;
      lcm_img.size =2*n_colors*isize;
    }else{
      std::cout << "Color Error\n"; 
    }    
    lcm_img.data.assign(singleimage_data, singleimage_data + ( n_colors*isize));
    lcm_publish_.publish(channel.c_str(), &lcm_img);
  }
}



int main(int argc, char **argv){
  ros::init(argc, argv, "ros2lcm_camera");
  ros::NodeHandle nh;
  App *app = new App(nh);
  std::cout << "ros2lcm translator ready\n";
  ROS_ERROR("Mono Camera Translator Sleeping");
  sleep(4);
  ROS_ERROR("Mono Camera Translator Ready");
  ros::spin();
  return 0;
}
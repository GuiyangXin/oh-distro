// renderer of lidar data in collections renderer
// uses pcl and can be used to dump cloud to file as PCD
// fully working as of august 2013

#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <Eigen/Dense>
#include <boost/shared_ptr.hpp>
#include <boost/assign/std/vector.hpp>

#include <lcm/lcm-cpp.hpp>

#include <bot_frames/bot_frames.h>
#include <bot_param/param_client.h>
#include <bot_param/param_util.h>
#include <bot_frames_cpp/bot_frames_cpp.hpp>

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/common/transforms.h>
#include <pointcloud_tools/pointcloud_vis.hpp> // visualize pt clds
#include <pointcloud_tools/pointcloud_lcm.hpp> // visualize pt clds
#include <image_io_utils/image_io_utils.hpp> // to simplify jpeg/zlib compression and decompression

#include "lcmtypes/bot_core.hpp"
#include "lcmtypes/multisense.hpp"
#include "lcmtypes/visualization.hpp"

#include <camera_params/camera_params.hpp>     // Camera Parameters
#include <ConciseArgs>

using namespace std;
using namespace Eigen;
using namespace boost::assign; // bring 'operator+()' into scope

struct CommandLineConfig
{
    int batch_size;
    std::string lidar_channel;
};


class CamData{
  public:
    CamData(std::string channel_ , BotParam* botparam):channel_(channel_){
      camera_params_.setParams(botparam, string("cameras." + channel_) );
      img_buf_= (uint8_t*) malloc(3* camera_params_.width  * camera_params_.height);
      img_received_=false;
      camtrans_ = bot_param_get_new_camtrans(botparam, channel_.c_str());
    };

    uint8_t* img_buf_;
    bool img_received_;
    CameraParams camera_params_;   
    string channel_;
    BotCamTrans *camtrans_;    
};


class Pass{
  public:
    Pass(boost::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_);
    
    ~Pass(){
    }    
  private:
    boost::shared_ptr<lcm::LCM> lcm_;
    const CommandLineConfig& cl_cfg_;
    
    void lidarHandler(const lcm::ReceiveBuffer* rbuf, 
                      const std::string& channel, const  bot_core::planar_lidar_t* msg);   
    void multisenseHandler(const lcm::ReceiveBuffer* rbuf, 
                           const std::string& channel, const  multisense::images_t* msg);   
    void imageHandler(const lcm::ReceiveBuffer* rbuf, 
                      const std::string& channel, const  bot_core::image_t* msg);   
    
    void colorizeLidar(int64_t utime, 
                            pcl::PointCloud<pcl::PointXYZRGB>::Ptr &scan_laser,
                           int cam_id);
    
    pointcloud_vis* pc_vis_ ;
    BotParam* botparam_;
    BotFrames* botframes_;
    bot::frames* botframes_cpp_;   
    
    int printf_counter_; // used for terminal feedback
    bool verbose_;
    
    image_io_utils*  imgutils_;    
    std::vector< CamData* > cams_;
};

Pass::Pass(boost::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_):
    lcm_(lcm_), cl_cfg_(cl_cfg_){
  botparam_ = bot_param_new_from_server(lcm_->getUnderlyingLCM(), 0);
  botframes_= bot_frames_get_global(lcm_->getUnderlyingLCM(), botparam_);
  
  lcm_->subscribe( cl_cfg_.lidar_channel  ,&Pass::lidarHandler,this);
  
  lcm_->subscribe( "CAMERACHEST_LEFT" ,&Pass::imageHandler,this);
  lcm_->subscribe( "CAMERACHEST_RIGHT" ,&Pass::imageHandler,this);
  lcm_->subscribe( "CAMERA" ,&Pass::multisenseHandler,this);

  cams_.push_back( new CamData( "CAMERACHEST_LEFT", botparam_  ) );
  cams_.push_back( new CamData( "CAMERACHEST_RIGHT", botparam_  ) );
  cams_.push_back( new CamData( "CAMERA_LEFT", botparam_  ) );
  
  
  bool reset =0;
  pc_vis_ = new pointcloud_vis( lcm_->getUnderlyingLCM() );
  // obj: id name type reset
  // pts: id name type reset objcoll usergb rgb
  pc_vis_->obj_cfg_list.push_back( obj_cfg(60000,"Pose - Laser",5,reset) );
  pc_vis_->ptcld_cfg_list.push_back( ptcld_cfg(60001,"Cloud - Laser"         ,1,reset, 60000,0, {0.0, 0.0, 1.0} ));
  pc_vis_->obj_cfg_list.push_back( obj_cfg(2000,"Pose - Camera",5,reset) );
  pc_vis_->ptcld_cfg_list.push_back( ptcld_cfg(2001,"Cloud - Camera"           ,1,reset, 2000,1, { 1.0, 1.0, 0.0} ));  
  pc_vis_->ptcld_cfg_list.push_back( ptcld_cfg(2002,"Cloud - Camera Color"           ,1,reset, 2000,0, { 1.0, 1.0, 0.0} ));  
  
  // allocate space larger than largest image message:
  imgutils_ = new image_io_utils( lcm_->getUnderlyingLCM(), 1500, 3*1500); 
  
  printf_counter_ =0;  
  verbose_=false;
}

// Receive and Decompress the Images:
void Pass::imageHandler(const lcm::ReceiveBuffer* rbuf, 
                        const std::string& channel, const  bot_core::image_t* msg){
  if (channel == "CAMERACHEST_LEFT"){
    imgutils_->decodeImageToRGB( msg,  cams_[0]->img_buf_ );
    cams_[0]->img_received_ = true;
  }else if (channel == "CAMERACHEST_RIGHT"){
    imgutils_->decodeImageToRGB( msg,  cams_[1]->img_buf_ );
    cams_[1]->img_received_ = true;
  }
}
void Pass::multisenseHandler(const lcm::ReceiveBuffer* rbuf, 
                        const std::string& channel, const  multisense::images_t* msg){
  imgutils_->decodeImageToRGB(&(msg->images[0]),  cams_[2]->img_buf_ );
  cams_[2]->img_received_ = true;
}

void Pass::colorizeLidar(int64_t utime, 
                              pcl::PointCloud<pcl::PointXYZRGB>::Ptr &scan_laser,
                              int cam_id){
  CamData* cam_info = cams_[cam_id];
  
  // 1. 
  Eigen::Isometry3d camera_to_local;
  botframes_cpp_->get_trans_with_utime( botframes_ , cam_info->channel_.c_str() , "local"  , utime, camera_to_local);
  Eigen::Isometry3d scan_to_camera;
  botframes_cpp_->get_trans_with_utime( botframes_ , "SCAN", cam_info->channel_.c_str()  , utime, scan_to_camera);
  
  // 2. Project the scan into camera frame:
  Eigen::Isometry3f scan_to_camera_f= scan_to_camera.cast<float>();
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr scan_camera (new pcl::PointCloud<pcl::PointXYZRGB> ());
  pcl::transformPointCloud (*scan_laser, *scan_camera,
      scan_to_camera_f.translation(), Eigen::Quaternionf(scan_to_camera_f.rotation())  );  
  /// scan_camera now contains the lidar returns as 3d points in the camera frame
  
  // 3. Plot scan from camera frame:
  if (verbose_){
    Isometry3dTime camera_to_local_T = Isometry3dTime(printf_counter_, camera_to_local );
    pc_vis_->pose_to_lcm_from_list(2000, camera_to_local_T);
    pc_vis_->ptcld_to_lcm_from_list(2001, *scan_camera, printf_counter_, printf_counter_);  
  }
  
  // 4. Project camera color onto point
  for (size_t i=0 ; i< scan_camera->points.size() ; i++){
    pcl::PointXYZRGB pt = scan_camera->points[i];
    double p[] = {pt.x, pt.y, pt.z};
    if(p[2] > 0){
      double uv[3];
      bot_camtrans_project_point( cam_info->camtrans_, p, uv );      
      
      if ( (uv[0] >= 0) && (uv[0] <= cam_info->camera_params_.width)){
        if ( (uv[1] >= 0) && (uv[1] <= cam_info->camera_params_.height)){
          int pixel = round(uv[1]) *cam_info->camera_params_.width + round(uv[0]);
          scan_laser->points[i].r = (float) cam_info->img_buf_[pixel*3];
          scan_laser->points[i].g = (float) cam_info->img_buf_[pixel*3+1];
          scan_laser->points[i].b = (float) cam_info->img_buf_[pixel*3+2];  
        }
      }
    }
  }
  
  // TODO: add back in visualization of cloud colored by single camera:
  //pc_vis_->ptcld_to_lcm_from_list(2002, *scan_camera, printf_counter_, printf_counter_);  
}


void Pass::lidarHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  bot_core::planar_lidar_t* msg){
  
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr scan_laser (new pcl::PointCloud<pcl::PointXYZRGB> ());
  
  // 1. Convert scan into simple XY point cloud:  
  double minRange =0.0; // consider everything - don't remove any points
  double maxRange = 30.0;
  double validBeamAngles[] ={-10,10}; 
  convertLidar(msg->ranges, msg->nranges, msg->rad0,
      msg->radstep, scan_laser, minRange, maxRange,
      validBeamAngles[0], validBeamAngles[1]);  
  
  // Plot original scan from lidar frame:
  // pc_vis_->ptcld_to_lcm_from_list(60001, *scan_laser, printf_counter_, printf_counter_);  

  // 3. Colorize, camera by camera - with head camera last
  for (size_t cam_id=0; cam_id< cams_.size()  ;cam_id++){
    if (cams_[cam_id]->img_received_){
      colorizeLidar(msg->utime, scan_laser, cam_id);
    }else{
      cout << "No "<< cams_[cam_id]->channel_ <<" image yet\n"; 
    }
  }
  
  // 4. Visualize the colorized scan:
  Eigen::Isometry3d scan_to_local;
  botframes_cpp_->get_trans_with_utime( botframes_ , "SCAN", "local"  , msg->utime, scan_to_local);
  Isometry3dTime scan_to_local_T = Isometry3dTime(printf_counter_, scan_to_local);
  pc_vis_->pose_to_lcm_from_list(60000, scan_to_local_T);
  pc_vis_->ptcld_to_lcm_from_list(60001, *scan_laser, printf_counter_, printf_counter_);  
  
  if (printf_counter_% cl_cfg_.batch_size ==0){
    cout << "Filtering: " <<  " "  << msg->utime << "\n";
    //  vs::reset_collections_t reset;
    //  lcm_->publish("RESET_COLLECTIONS", &reset);    
    printf_counter_=0;
  }
  printf_counter_++;
}


int main( int argc, char** argv ){
  CommandLineConfig cl_cfg;
  cl_cfg.batch_size =200;
  cl_cfg.lidar_channel = "SCAN";
  
  ConciseArgs opt(argc, (char**)argv);
  opt.add(cl_cfg.lidar_channel, "l", "lidar_channel","lidar_channel");
  opt.add( cl_cfg.batch_size, "s", "size","Batch Size");
  opt.parse();
  std::cout << "lidar_channel: " << cl_cfg.lidar_channel << "\n"; 
  std::cout << "size: " << cl_cfg.batch_size<< "\n"; 
  
  boost::shared_ptr<lcm::LCM> lcm(new lcm::LCM);
  if(!lcm->good()){
    std::cerr <<"ERROR: lcm is not good()" <<std::endl;
  }
  
  Pass app(lcm, cl_cfg);
  cout << "Ready to filter lidar points" << endl << "============================" << endl;
  while(0 == lcm->handle());
  return 0;
}

#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <Eigen/Dense>
#include <boost/shared_ptr.hpp>

#include <lcm/lcm-cpp.hpp>

#include <bot_lcmgl_client/lcmgl.h>
#include <GL/gl.h>

#include <bot_frames/bot_frames.h>
#include <bot_param/param_client.h>
#include <bot_param/param_util.h>

#include "lcmtypes/bot_core.hpp"
#include "lcmtypes/multisense.hpp"

#include <ConciseArgs>

// define the following in order to eliminate the deprecated headers warning
//#define VTK_EXCLUDE_STRSTREAM_HEADERS
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/PolygonMesh.h>
#include <pcl/io/vtk_lib_io.h>
#include <pronto_utils/pronto_vis.hpp> // visualize pt clds

#include <image_io_utils/image_io_utils.hpp> // to simplify jpeg/zlib compression and decompression
#include <rgbd_simulation/rgbd_primitives.hpp>

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace std;
using namespace Eigen;

struct CommandLineConfig
{
    std::string lidar_channel;
};

class Pass{
  public:
    Pass(boost::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_);
    
    ~Pass(){
    }    
    
    void doWork();
    
  private:
    boost::shared_ptr<lcm::LCM> lcm_;
    const CommandLineConfig& cl_cfg_;
    
   
    int get_trans_with_utime(std::string from_frame, std::string to_frame, 
                                 int64_t utime, Eigen::Isometry3d & mat);
    
    
    bot_lcmgl_t* lcmgl_;
    BotParam* botparam_;
    BotFrames* botframes_;
    
    int printf_counter_; // used for terminal feedback
    Eigen::Isometry3d scan_to_local_; // transform used to do conversion
    std::vector<Eigen::Vector3d> cloud_transformed_; // incoming lidar converted to 3D points
    std::vector<Eigen::Vector3d> color_; 
    void drawPoints(int64_t last_utime);  
    
       image_io_utils*  imgutils_;    
    uint8_t* left_buf_;

    boost::shared_ptr<rgbd_primitives>  prim_;
    

    std::vector<Eigen::Vector3d> other_cloud_; 
    std::vector<Eigen::Vector3d> other_color_; 
    
};

Pass::Pass(boost::shared_ptr<lcm::LCM> &lcm_, const CommandLineConfig& cl_cfg_):
    lcm_(lcm_), cl_cfg_(cl_cfg_){
  lcmgl_= bot_lcmgl_init(lcm_->getUnderlyingLCM(), "lidar-to-3dpoints");
  botparam_ = bot_param_new_from_server(lcm_->getUnderlyingLCM(), 0);
  botframes_= bot_frames_get_global(lcm_->getUnderlyingLCM(), botparam_);
  
  printf_counter_ =0;  
  
      left_buf_ = (uint8_t*) malloc(1*1024* 1280);
      imgutils_ = new image_io_utils( lcm_->getUnderlyingLCM(), 1024, 3*1280); // extra space for stereo tasks

      scan_to_local_.setIdentity();
}


// from pronto_lcm
void convertLidar(std::vector< float > ranges, int numPoints, double thetaStart,
        double thetaStep,
        std::vector<Eigen::Vector3d> &cloud,
        double minRange = 0., double maxRange = 1e10,
        double validRangeStart = -1000, double validRangeEnd = 1000)
{
  double theta = thetaStart;
  for (int i = 0; i < numPoints; i++) {
    if (ranges[i] > minRange && ranges[i] < maxRange && theta > validRangeStart
            && theta < validRangeEnd) { 
      Eigen::Vector3d pt;
      pt(0) = ranges[i] * cos(theta);
      pt(1) = ranges[i] * sin(theta);
      pt(2) = 0;
      cloud.push_back(pt);
    }
    theta += thetaStep;
  }
}


int Pass::get_trans_with_utime(std::string from_frame, std::string to_frame, 
                                 int64_t utime, Eigen::Isometry3d & mat){
  int status;
  double matx[16];
  status = bot_frames_get_trans_mat_4x4_with_utime( botframes_, from_frame.c_str(),  to_frame.c_str(), utime, matx);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      mat(i,j) = matx[i*4+j];
    }
  }  
  return status;
}



void bot_lcmgl_draw_axes(bot_lcmgl_t * lcmgl, const Eigen::Affine3d & trans, double scale)
{
  lcmglPushMatrix();
  lcmglMultMatrixd(trans.data());
  lcmglScalef(scale, scale, scale);
  bot_lcmgl_draw_axes(lcmgl);
  lcmglPopMatrix();
}

void Pass::drawPoints(int64_t last_utime){
  // a. Visualize the axes of interest:
  bot_lcmgl_push_matrix(lcmgl_);
  bot_lcmgl_rotated(lcmgl_, -90, 1, 0, 0);  
  
  
  bot_lcmgl_draw_axes(lcmgl_, scan_to_local_, 4);
  
  if(1==1){
  // b. Plot scan from camera frame:
  bot_lcmgl_point_size(lcmgl_, 4.5f);
  bot_lcmgl_begin(lcmgl_, GL_POINTS);  // render as points
//  bot_lcmgl_color3f(lcmgl_, 0, 0, 1); // Blue
  for (size_t i=0; i < cloud_transformed_.size(); ++i) {
    
    Eigen::Vector3d c = color_[i];
    bot_lcmgl_color3f(lcmgl_, c(0)/255, c(1)/255, c(2)/255); // Blue
    
    Eigen::Vector3d pt = cloud_transformed_[i];
    bot_lcmgl_vertex3d(lcmgl_, pt(0), pt(1), pt(2)); // 100 points in line
  }
  bot_lcmgl_end(lcmgl_);
  }
  
  if(1==1){
  // b. Plot scan from camera frame:
  bot_lcmgl_point_size(lcmgl_, 4.5f);
  bot_lcmgl_begin(lcmgl_, GL_POINTS);  // render as points
  bot_lcmgl_color3f(lcmgl_, 0, 0, 1); // Blue
  for (size_t i=0; i < other_cloud_.size(); ++i) {

    Eigen::Vector3d c = other_color_[i];
    bot_lcmgl_color3f(lcmgl_, c(0)/255, c(1)/255, c(2)/255); // Blue
    
    
    Eigen::Vector3d pt = other_cloud_[i];
    
    bot_lcmgl_vertex3d(lcmgl_, pt(0), pt(1), pt(2)); // 100 points in line
    //bot_lcmgl_vertex3d(lcmgl_, 1,1,1); // 100 points in line
  }
  bot_lcmgl_end(lcmgl_);  
  }
  
  bot_lcmgl_pop_matrix(lcmgl_);  
  bot_lcmgl_switch_buffer(lcmgl_);  

  std::cout << other_cloud_.size() << " other cloud\n";
  if (printf_counter_%80 ==0){
    cout << "Scan @ "  << last_utime << "\n";
  }
  printf_counter_++;
}


void Pass::doWork( ){
  

  pcl::PolygonMesh mesh;
  pcl::io::loadPolygonFile(  "atlas_one_mesh.ply"    ,mesh);
  pcl::PolygonMesh::Ptr mesh_ptr (new pcl::PolygonMesh(mesh));
  cout << "read in ply\n"; 
  
  

  //pcl::PointCloud<pcl::PointXYZRGB>::Ptr mesh_cloud;
  //pcl::fromPCLPointCloud2(mesh.cloud, *mesh_cloud);
  
  
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr mesh_cloud =prim_->sampleMesh(mesh_ptr, 100000); // NB: disc sampled for both top and bottom lid - givine twice the density
  
  
  Eigen::Isometry3d pose_out = Eigen::Isometry3d::Identity();
  
  pose_out.translation()  << -0.15,1.4,-1.15;
  Eigen::Quaterniond quat = euler_to_quat(90*M_PI/180, -20*M_PI/180, 0 );
  //90.0*M_PI/180.0, -90.0*M_PI/180.0, 0.0*M_PI/180.0
  pose_out.rotate( quat);   
  
  Eigen::Isometry3f visual_origin_f= pose_out.cast<float>();
  Eigen::Quaternionf visual_origin_quat(visual_origin_f.rotation());
  pcl::transformPointCloud (*mesh_cloud, *mesh_cloud, 
                            visual_origin_f.translation(), visual_origin_quat);  
  
  
  std::cout << "doWork\n";
  BotCamTrans *camtrans;
  
  camtrans = bot_param_get_new_camtrans(botparam_, "CAMERACHEST_LEFT");
  
  int height = 1280;
  int width = 1024;
  
  cloud_transformed_.clear();
  color_.clear();
  
  other_cloud_.clear();
  other_color_.clear();
  
  
  cv::Mat img = cv::Mat::zeros(height,width,CV_8UC1); // h,w
  img.data = left_buf_;
  
  
  for (size_t i=0; i < mesh_cloud->points.size();    i++) {
      
      Eigen::Vector3d pt( mesh_cloud->points[i].x,
                          mesh_cloud->points[i].y, 
                          mesh_cloud->points[i].z + 1) ;
      other_cloud_.push_back(pt);
      
      
      double p[] = { mesh_cloud->points[i].x,
                      mesh_cloud->points[i].y,
                   mesh_cloud->points[i].z +1      };
                   
      double im_xyz[3];
      bot_camtrans_project_point(camtrans, p, im_xyz );      
      // std::cout<< im_xyz[0] << " " << im_xyz[1] <<" " << im_xyz[2]<<"\n";
      
      if (( im_xyz[0] > 0 ) && ( im_xyz[0] < width)){
        if (( im_xyz[1] > 0 ) && ( im_xyz[1] < height)){
          //int pixel = round(im_xyz[1]) *width + round(im_xyz[0]);
          //left_buf_[pixel] = 100;
          
          cv::circle( img, cv::Point( im_xyz[0], im_xyz[1] ),
            8.0,         100,         -1,         8 );
      
      
        }
      }

      Eigen::Vector3d c(0,0,0);
      other_color_.push_back(c);
  }    
  
  
  for (int y = 0; y < height; y=y+4) {
    for (int x = 0; x < width; x=x+4) {
      double ray_cam[3];
      bot_camtrans_unproject_pixel(camtrans, x, y, ray_cam);
      
      Eigen::Vector3d pt(2.5*ray_cam[0], 2.5*ray_cam[1], 2.5*ray_cam[2] ) ;
      cloud_transformed_.push_back(pt);
      
      int pixel = y*width + x;
      
      Eigen::Vector3d c( left_buf_[pixel],
        left_buf_[pixel], left_buf_[pixel] ) ;
      color_.push_back(c);
    }
  }
  
  
  cv::imwrite("image_out.png", img);     
  
  
  
  /*
  for (double y = -30; y < 30; y=y+0.1) {
    for (double x = -40; x < 40; x=x+0.1) {
      
      Eigen::Vector3d pt(x,y, 5.0 ) ;
      other_cloud_.push_back(pt);
      
      double p[] = {x,y,5.0};
      double im_xyz[3];
      bot_camtrans_project_point(camtrans, p, im_xyz );      
      //std::cout<< im_xyz[0] << " " << im_xyz[1] <<" " << im_xyz[2]<<"\n";
      Eigen::Vector3d pt2(im_xyz[0] ,im_xyz[1], im_xyz[2] ) ;
      //other_cloud_.push_back(pt2);
      
      int pixel = round(im_xyz[1]) *width + round(im_xyz[0]);
      
      Eigen::Vector3d c( left_buf_[pixel],
        left_buf_[pixel], left_buf_[pixel] ) ;
      
      other_color_.push_back(c);
    }
    std::cout << y << " b\n";
  }  
  */
  
  drawPoints(0);
}


int main( int argc, char** argv ){
  CommandLineConfig cl_cfg;
  cl_cfg.lidar_channel = "CAMERACHEST_LEFT";
  ConciseArgs opt(argc, (char**)argv);
  opt.add(cl_cfg.lidar_channel, "l", "lidar_channel","lidar_channel");
  opt.parse();
  std::cout << "lidar_channel: " << cl_cfg.lidar_channel << "\n"; 
  
  boost::shared_ptr<lcm::LCM> lcm(new lcm::LCM);
  if(!lcm->good()){
    std::cerr <<"ERROR: lcm is not good()" <<std::endl;
  }
  
  Pass app(lcm, cl_cfg);
  
 // while(0 == lcm->handle());
  
  app.doWork();
  
  return 0;
}

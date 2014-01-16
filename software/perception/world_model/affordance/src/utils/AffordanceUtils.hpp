#ifndef AFFORDANCE_UTILS_
#define AFFORDANCE_UTILS_

#include <iostream>
#include <string>
#include <Eigen/Dense>
#include <Eigen/StdVector>


// define the following in order to eliminate the deprecated headers warning
//#define VTK_EXCLUDE_STRSTREAM_HEADERS
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
//#include <pcl/io/vtk_lib_io.h>
#include "pcl/PolygonMesh.h"
#include <pcl/common/transforms.h>

// define the following in order to eliminate the deprecated headers warning
#define VTK_EXCLUDE_STRSTREAM_HEADERS
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/io/vtk_lib_io.h>
#include "pcl/PolygonMesh.h"
#include <pcl/common/transforms.h>

#include <rgbd_simulation/rgbd_primitives.hpp>
#include <lcmtypes/drc_lcmtypes.hpp>


class AffordanceUtils {
  public:
    AffordanceUtils ();

    Eigen::Isometry3d getPose(double xyz[3], double rpy[3]);
    //std::vector<std::string> param_names, std::vector<double> params );

    // just return the raw points:
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr getCloudFromAffordance(std::vector< std::vector< float > > &points);
    
    // sample the mesh:
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr getCloudFromAffordance(std::vector< std::vector< float > > &points,
          std::vector< std::vector< int > > &triangles, double pts_per_msquared);
    
    pcl::PolygonMesh::Ptr getMeshFromAffordance(std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles, Eigen::Isometry3d & transform);
    
    pcl::PolygonMesh::Ptr getMeshFromAffordance(std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles);
    
    
    ////// Conversion from/to Plane from XYZYPR affordance params
    void setPlaneFromXYZYPR(double xyz[3], double rpy[3], 
                   std::vector<float> &plane_coeffs, Eigen::Vector3d &plane_centroid);
    
    void setXYZRPYFromPlane(double xyz[3], double rpy[3], 
                   std::vector<float> plane_coeffs, Eigen::Vector3d plane_centroid);
    
    void setXYZRPYFromIsometry3d(double xyz[3], double rpy[3], 
                   Eigen::Isometry3d &pose);
    
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr getBoundingBoxCloud(double bounding_xyz[], double bounding_rpy[], double bounding_lwh[]);

    void printAffordance(std::vector<std::string> &param_names, std::vector<double> &params,
                         std::stringstream &ss){
      for (size_t j=0; j< param_names.size(); j++){
        ss << param_names[j] << ": " << params[j] << " | ";
      }      
    }
    
    bool getMeshAsLists(std::string filename,
                  std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles);
    
    bool getCloudAsLists(std::string filename,
                  std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles);
    
    bool getModelAsLists(std::string filename,
                  std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles){
      bool found_file=false;
      int length = filename.length() ;
      std::string extension = filename.substr (length-3,3);
      if (extension=="ply"){
        found_file = getMeshAsLists(filename , points, triangles);
      }else if(extension=="pcd"){
        found_file = getCloudAsLists(filename , points, triangles);
      }else{
        std::cout << "I don't understand this filename " << filename << "\n";
        found_file=false;
      }
      return found_file;
    }
    
    
    // Create an aff plus message:
    drc::affordance_plus_t getBoxAffordancePlus(int uid, std::string friendly_name, Eigen::Isometry3d position , std::vector<double> &lengths);
    
    
  private:
    boost::shared_ptr<rgbd_primitives>  prim_;
    
};



#endif

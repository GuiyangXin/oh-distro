#include <map>
#include <boost/assign/std/vector.hpp>

#include "AffordanceUtils.hpp"

using namespace Eigen;
using namespace std;
using namespace boost::assign; // bring 'operator+()' into scope


AffordanceUtils::AffordanceUtils() {
  
}

Eigen::Isometry3d AffordanceUtils::getPose(double xyz[3], double rpy[3]){
//  std::vector<std::string> param_names, std::vector<double> params ){
  /*
  std::map<std::string,double> am;
  for (size_t j=0; j< param_names.size(); j++){
    am[ param_names[j] ] = params[j];
  }
  
  // Convert Euler to Isometry3d:
  Matrix3d m;
  m = AngleAxisd (am.find("yaw")->second, Vector3d::UnitZ ())
                  * AngleAxisd (am.find("pitch")->second, Vector3d::UnitY ())
                  * AngleAxisd (am.find("roll")->second, Vector3d::UnitX ());  
  Eigen::Isometry3d pose =  Eigen::Isometry3d::Identity();
  pose *= m;  
  pose.translation()  << am.find("x")->second , am.find("y")->second, am.find("z")->second;
  */
  Matrix3d m;
  m = AngleAxisd ( rpy[2], Vector3d::UnitZ ())
                  * AngleAxisd (rpy[1] , Vector3d::UnitY ())
                  * AngleAxisd ( rpy[0] , Vector3d::UnitX ());  
  Eigen::Isometry3d pose =  Eigen::Isometry3d::Identity();
  pose *= m;  
  pose.translation()  << xyz[0], xyz[1], xyz[2];  
  
  return pose;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr AffordanceUtils::getCloudFromAffordance(std::vector< std::vector< float > > &points){
  
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr pts (new pcl::PointCloud<pcl::PointXYZRGB> ());
  for (size_t i=0; i < points.size(); i++){ 
    pcl::PointXYZRGB pt;
    pt.x = points[i][0];
    pt.y = points[i][1];
    pt.z = points[i][2];
    pts->points.push_back(pt);
  }
  cout << pts->points.size() << " points extracted [converted]\n";
  pts->width = 1; pts->height = pts->size(); // to prevent "Number of points different than width * height!"
  return pts;
}


pcl::PointCloud<pcl::PointXYZRGB>::Ptr AffordanceUtils::getCloudFromAffordance(std::vector< std::vector< float > > &points,
                      std::vector< std::vector< int > > &triangles, double pts_per_msquared){
  pcl::PolygonMesh::Ptr mesh = getMeshFromAffordance(points,triangles);
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr pts =prim_->sampleMesh(mesh, pts_per_msquared);
  cout << pts->points.size() << " points extracted [sampled] at " <<  pts_per_msquared << " psm\n";
  pts->width = 1; pts->height = pts->size(); // to prevent "Number of points different than width * height!"
  return pts;
}


pcl::PolygonMesh::Ptr AffordanceUtils::getMeshFromAffordance(std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles, Eigen::Isometry3d & transform){
  
  pcl::PolygonMesh::Ptr mesh = getMeshFromAffordance(points, triangles);
  
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZRGB> ());
  pcl::fromROSMsg(mesh->cloud, *cloud);  
  
  // Adjust object to be centered on z-axis (standard used by URDF)
//  pcl::transformPointCloud (*cloud, *cloud,
//        Eigen::Vector3f(0,0, -height/2.0), Eigen::Quaternionf(1.0, 0.0,0.0,0.0)); // !! modifies cloud
    
  Eigen::Isometry3f pose_f = transform.cast<float>();
  Eigen::Quaternionf quat_f(pose_f.rotation());
  pcl::transformPointCloud (*cloud, *cloud,
      pose_f.translation(), quat_f); // !! modifies cloud
  
  pcl::toROSMsg(*cloud, mesh->cloud);  
  return mesh;
}

pcl::PolygonMesh::Ptr AffordanceUtils::getMeshFromAffordance(std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles){
  pcl::PolygonMesh mesh;   
  pcl::PolygonMesh::Ptr mesh_ptr (new pcl::PolygonMesh (mesh));  
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr pts (new pcl::PointCloud<pcl::PointXYZRGB> ());
  
  for (size_t i=0; i < points.size(); i++){ 
    pcl::PointXYZRGB pt;
    pt.x = points[i][0];
    pt.y = points[i][1];
    pt.z = points[i][2];
    pts->points.push_back(pt);
  }
  pcl::toROSMsg (*pts, mesh_ptr->cloud);
  
  vector <pcl::Vertices> verts;
  for(size_t i=0; i<  triangles.size (); i++){ // each triangle/polygon
    pcl::Vertices poly;
    std::vector <unsigned int> v ( triangles[i].begin(), triangles[i].end()) ;
    poly.vertices = v;
    verts.push_back(poly);
  }
  mesh_ptr->polygons = verts;
  
  return mesh_ptr;
}

void AffordanceUtils::setPlaneFromXYZYPR(double xyz[3], double rpy[3], 
                std::vector<float> &plane_coeffs, Eigen::Vector3d &plane_centroid){
  // Ridiculously hacky way of converting from plane affordance to plane coeffs.
  // the x-direction of the plane pose is along the axis - hence this
  Matrix3d m;
  m = AngleAxisd ( rpy[2], Vector3d::UnitZ ())
                  * AngleAxisd (rpy[1] , Vector3d::UnitY ())
                  * AngleAxisd ( rpy[0] , Vector3d::UnitX ());  
  Eigen::Isometry3d transform =  Eigen::Isometry3d::Identity();
  transform *= m;  
  transform.translation()  << xyz[0], xyz[1], xyz[2];    
  

  Eigen::Isometry3d ztransform;
  ztransform.setIdentity();
  ztransform.translation()  << 0 ,0, 1; // determine a point 1m in the z direction... use this as the normal
  ztransform = transform*ztransform;
  float a =(float) ztransform.translation().x() -  transform.translation().x();
  float b =(float) ztransform.translation().y() -  transform.translation().y();
  float c =(float) ztransform.translation().z() -  transform.translation().z();
  float d = - (a*xyz[0] + b*xyz[1] + c*xyz[2]);
  plane_coeffs.clear();
  plane_coeffs += a, b, c, d;
  /*
  cout << "pitch : " << 180.*am.find("pitch")->second/M_PI << "\n";
  cout << "yaw   : " << 180.*am.find("yaw")->second/M_PI << "\n";
  cout << "roll   : " << 180.*am.find("roll")->second/M_PI << "\n";   
  obj_cfg oconfig = obj_cfg(1251000,"Tracker | Affordance Pose Z",5,1);
  Isometry3dTime reinit_poseT = Isometry3dTime ( 0, ztransform );
  pc_vis_->pose_to_lcm(oconfig,reinit_poseT);
  */

  plane_centroid=  Eigen::Vector3d( xyz[0], xyz[1], xyz[2]); // last element held at zero
}

void AffordanceUtils::setXYZRPYFromPlane(double xyz[3], double rpy[3], 
                std::vector<float> plane_coeffs, Eigen::Vector3d plane_centroid){
  
  double run = sqrt(plane_coeffs[0]*plane_coeffs[0] + plane_coeffs[1]*plane_coeffs[1] 
                        +  plane_coeffs[2]*plane_coeffs[2]);
  double yaw = atan2 ( plane_coeffs[1] , plane_coeffs[0]);
  double pitch = acos( plane_coeffs[2]/ run);
  // Conversion from Centroid+Plane to XYZRPY is not constrained
  // - properly set to zero
  double roll =0; 

  xyz[0] = plane_centroid(0);
  xyz[1] = plane_centroid(1);
  xyz[2] = plane_centroid(2);
  rpy[0] = roll;
  rpy[1] = pitch;
  rpy[2] = yaw;
  
/*  for (size_t j=0; j< param_names.size(); j++){
    if (param_names[j] == "x"){
      params[j] = plane_centroid(0);
    }else if(param_names[j] == "y"){
      params[j] = plane_centroid(1);
    }else if(param_names[j] == "z"){
      params[j] = plane_centroid(2);
    }else if(param_names[j] == "yaw"){
      params[j] = yaw;
    }else if(param_names[j] == "pitch"){
      params[j] = pitch;
    }else if(param_names[j] == "roll"){
      params[j] = roll;
    }
  }
*/  
}


/// This function replicates one in pointcloud_math. But does a function exist in Eigen?
Eigen::Quaterniond euler_to_quat(double roll, double pitch, double yaw) {
  double sy = sin(yaw*0.5);
  double cy = cos(yaw*0.5);
  double sp = sin(pitch*0.5);
  double cp = cos(pitch*0.5);
  double sr = sin(roll*0.5);
  double cr = cos(roll*0.5);
  double w = cr*cp*cy + sr*sp*sy;
  double x = sr*cp*cy - cr*sp*sy;
  double y = cr*sp*cy + sr*cp*sy;
  double z = cr*cp*sy - sr*sp*cy;
  return Eigen::Quaterniond(w,x,y,z);
}


/// This function replicates one in pointcloud_math. But does a function exist in Eigen?
void quat_to_euler(Eigen::Quaterniond q, double& roll, double& pitch, double& yaw) {
  const double q0 = q.w();
  const double q1 = q.x();
  const double q2 = q.y();
  const double q3 = q.z();
  roll = atan2(2*(q0*q1+q2*q3), 1-2*(q1*q1+q2*q2));
  pitch = asin(2*(q0*q2-q3*q1));
  yaw = atan2(2*(q0*q3+q1*q2), 1-2*(q2*q2+q3*q3));
}


void AffordanceUtils::setXYZRPYFromIsometry3d(double xyz[3], double rpy[3], 
                   Eigen::Isometry3d &pose){
  Eigen::Quaterniond r(pose.rotation());
  double yaw, pitch, roll;
  quat_to_euler(r, roll, pitch, yaw);  

  xyz[0] = pose.translation().x();
  xyz[1] = pose.translation().y();
  xyz[2] = pose.translation().z();
  
  
  rpy[0] = roll;
  rpy[1] = pitch;
  rpy[2] = yaw;
}  



pcl::PointCloud<pcl::PointXYZRGB>::Ptr AffordanceUtils::getBoundingBoxCloud(double bounding_xyz[], double bounding_rpy[], double bounding_lwh[]){
  
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr bb_pts (new pcl::PointCloud<pcl::PointXYZRGB> ());
  pcl::PointXYZRGB pt1, pt2, pt3, pt4, pt5, pt6, pt7, pt8;
  pt1.x = -bounding_lwh[0]/2;       pt1.y = -bounding_lwh[1]/2;    pt1.z = -bounding_lwh[2]/2;  
  pt2.x = -bounding_lwh[0]/2;       pt2.y = -bounding_lwh[1]/2;    pt2.z = bounding_lwh[2]/2;  

  pt3.x = -bounding_lwh[0]/2;       pt3.y = bounding_lwh[1]/2;    pt3.z = -bounding_lwh[2]/2;  
  pt4.x = -bounding_lwh[0]/2;       pt4.y = bounding_lwh[1]/2;    pt4.z = bounding_lwh[2]/2;  

  pt5.x = bounding_lwh[0]/2;       pt5.y = -bounding_lwh[1]/2;    pt5.z = -bounding_lwh[2]/2;  
  pt6.x = bounding_lwh[0]/2;       pt6.y = -bounding_lwh[1]/2;    pt6.z = bounding_lwh[2]/2;  

  pt7.x = bounding_lwh[0]/2;       pt7.y = bounding_lwh[1]/2;    pt7.z = -bounding_lwh[2]/2;  
  pt8.x = bounding_lwh[0]/2;       pt8.y = bounding_lwh[1]/2;    pt8.z = bounding_lwh[2]/2; 

  // z-dir
  bb_pts->points.push_back(pt1);      bb_pts->points.push_back(pt2);    
  bb_pts->points.push_back(pt3);      bb_pts->points.push_back(pt4);    
  bb_pts->points.push_back(pt5);  bb_pts->points.push_back(pt6);
  bb_pts->points.push_back(pt7);  bb_pts->points.push_back(pt8);
  // x-dir
  bb_pts->points.push_back(pt1);  bb_pts->points.push_back(pt5);   
  bb_pts->points.push_back(pt2);  bb_pts->points.push_back(pt6);    
  bb_pts->points.push_back(pt3);  bb_pts->points.push_back(pt7);    
  bb_pts->points.push_back(pt4);  bb_pts->points.push_back(pt8);    
  // y-dir
  bb_pts->points.push_back(pt1);  bb_pts->points.push_back(pt3);   
  bb_pts->points.push_back(pt2);  bb_pts->points.push_back(pt4);    
  bb_pts->points.push_back(pt5);  bb_pts->points.push_back(pt7);    
  bb_pts->points.push_back(pt6);  bb_pts->points.push_back(pt8);    

  
  Eigen::Isometry3d bb_pose_ = getPose( bounding_xyz, bounding_rpy );
  Eigen::Quaternionf bb_quat_f(bb_pose_.cast<float>().rotation()  );
  pcl::transformPointCloud (*bb_pts, *bb_pts,
        bb_pose_.cast<float>().translation() , bb_quat_f); // !! modifies cloud

  return bb_pts;
}




bool AffordanceUtils::getMeshAsLists(std::string filename,
                  std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles){ 

  pcl::PolygonMesh combined_mesh;     // (new pcl::PolygonMesh);
  pcl::io::loadPolygonFile (filename, combined_mesh);
  pcl::PolygonMesh::Ptr mesh (new pcl::PolygonMesh (combined_mesh));

  pcl::PointCloud<pcl::PointXYZRGB> newcloud;
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_ptr (new pcl::PointCloud<pcl::PointXYZRGB> ());
  pcl::fromROSMsg(mesh->cloud, *cloud_ptr );  

  /*
  moveCloud(cloud_ptr);
  scaleCloud(cloud_ptr);
  
  
  pcl::toROSMsg (*cloud_ptr, mesh->cloud);
  pcl::io::savePolygonFile("drill_pclio.ply", *mesh);
  savePLYFile(mesh, "drill_mfallonio.ply");
  */
  
  for (size_t i=0; i < cloud_ptr->points.size(); i++){ 
    Eigen::Vector4f tmp = cloud_ptr->points[i].getVector4fMap();
    std::vector<float> point;
    point.push_back ( (float) tmp(0)  );
    point.push_back ( (float) tmp(1)  );
    point.push_back ( (float) tmp(2)  );
    points.push_back(point);
  }
  
  for(size_t i=0; i<  mesh->polygons.size (); i++){ // each triangle/polygon
    pcl::Vertices poly = mesh->polygons[i];//[i];
    if (poly.vertices.size() > 3){
      cout << "poly " << i << " is of size " << poly.vertices.size() << " lcm cannot support this\n";
      exit(-1); 
    }
    vector<int> triangle(poly.vertices.begin(), poly.vertices.end());
    triangles.push_back(triangle );
  }
  cout << "Read a mesh with " << points.size() << " points and " << triangles.size() << " triangles\n";
  return true;
}    
    
    
bool AffordanceUtils::getCloudAsLists(std::string filename,
                  std::vector< std::vector< float > > &points, 
                  std::vector< std::vector< int > > &triangles){    
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_ptr (new pcl::PointCloud<pcl::PointXYZRGB> ());
  
    if (pcl::io::loadPCDFile<pcl::PointXYZRGB> ( filename, *cloud_ptr) == -1){ // load the file
      cout << "getCloudAsLists() couldn't read " << filename << ", quitting\n";
      exit(-1);
    } 
    
  for (size_t i=0; i < cloud_ptr->points.size(); i++){ 
    Eigen::Vector4f tmp = cloud_ptr->points[i].getVector4fMap();
    std::vector<float> point;
    point.push_back ( (float) tmp(0)  );
    point.push_back ( (float) tmp(1)  );
    point.push_back ( (float) tmp(2)  );
    points.push_back(point);
  }
      
  cout << "Read a point cloud with " << points.size() << " points\n";
  return true;
}




drc::affordance_plus_t AffordanceUtils::getBoxAffordancePlus(int uid, std::string friendly_name, Eigen::Isometry3d position , std::vector<double> &lengths){ 
  drc::affordance_t a;
  a.utime =0;
  a.map_id =0;
  a.uid =uid;
  a.otdf_type ="box";
  a.aff_store_control = drc::affordance_t::NEW;

  a.param_names.push_back("lX");      a.params.push_back(lengths[0]);
  a.param_names.push_back("lY");      a.params.push_back(lengths[1]);
  a.param_names.push_back("lZ");      a.params.push_back(lengths[2]);
  a.nparams = a.params.size();
  a.nstates =0;
  
  a.origin_xyz[0]=position.translation().x(); a.origin_xyz[1]=position.translation().y(); a.origin_xyz[2]= position.translation().z(); 
  
  Eigen::Quaterniond quat(position.rotation());
  quat_to_euler( quat, a.origin_rpy[0] , a.origin_rpy[1], a.origin_rpy[2] ) ;
  
  drc::affordance_plus_t a1;
  a1.aff = a;
  a1.npoints=0; 
  a1.ntriangles =0;

  return a1;
}


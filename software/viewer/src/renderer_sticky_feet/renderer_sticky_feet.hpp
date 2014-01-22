#ifndef RENDERER_STICKYFEET_HPP
#define RENDERER_STICKYFEET_HPP

#include <iostream>
#include <boost/function.hpp>

#include <lcm/lcm.h>
#include <lcm/lcm-cpp.hpp>
#include "lcmtypes/drc/data_request_t.hpp"
#include "lcmtypes/drc/utime_t.hpp"
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <bot_vis/bot_vis.h>
#include <bot_frames/bot_frames.h>
#include <bot_core/rotations.h>
#include <gdk/gdkkeysyms.h>
#include <Eigen/Dense>

#include <visualization_utils/angles.hpp>
#include <visualization_utils/eigen_kdl_conversions.hpp>
#include "FootStepPlanListener.hpp"

#include <maps/ViewClient.hpp>
#include <maps/BotWrapper.hpp>

using namespace std;
using namespace boost;
using namespace Eigen;
using namespace visualization_utils;

namespace renderer_sticky_feet{


  struct PerceptionData {
    maps::ViewClient mViewClient;
    maps::BotWrapper::Ptr mBotWrapper;
  };

  typedef struct _RendererStickyFeet
  {
    BotRenderer renderer;
    BotViewer *viewer;
    BotGtkParamWidget *pw;
    boost::shared_ptr<FootStepPlanListener> footStepPlanListener;
    boost::shared_ptr<lcm::LCM> lcm;
    int64_t max_draw_utime;
    BotEventHandler ehandler;
    bool show_detailed_info;
    bool clicked;
    bool dragging;
    
    PerceptionData *perceptionData;

    Eigen::Vector3f ray_start;
    Eigen::Vector3f ray_end;
    Eigen::Vector3f ray_hit;
    Eigen::Vector3f ray_hit_drag;
    Eigen::Vector3f prev_ray_hit_drag;
    Eigen::Vector3f marker_offset_on_press;// maintains this offset while dragging
    double ray_hit_t;
    
    
    std::string* selection;
    std::string* marker_selection;
    int selected_planned_footstep_index;

    
    // Our only source of a free running clock:
    int64_t robot_utime;
    GtkWidget *plan_approval_dock;
    GtkWidget *plan_execution_dock;
    GtkWidget *plan_execute_button;
  } RendererStickyFeet;

  inline static bool get_support_surface_height_from_perception(void *user, Eigen::Vector3f &queryPt, double &height)
  {
    RendererStickyFeet *self = (RendererStickyFeet*) user;
   
    int iViewId = drc::data_request_t::HEIGHT_MAP_SCENE;
    maps::ViewClient::ViewPtr view = self->perceptionData->mViewClient.getView(iViewId);
    
    
    // get average height mean.
    /*float zMean = 0;
    if (view != NULL) {
      maps::PointCloud::Ptr cloud = view->getAsPointCloud();
      for (int i = 0; i < cloud->size(); ++i) {
        zMean += cloud->points[i].z;
      }
      if (cloud->size() > 0) {
        zMean /= cloud->size();
      }
    }*/

    //Eigen::Vector3f queryPt(query_x,query_y,zMean);  
    
    Eigen::Vector3f closestPt,closestNormal;
    closestPt = queryPt;
    closestNormal<< 0,0,1;   
    height = queryPt[2];
    
    if (view != NULL) {
      if(!view->getClosest(queryPt,closestPt,closestNormal))
      {
        closestPt = queryPt;
        closestNormal<< 0,0,1;
	height = queryPt[2];
        return false;
      }

      height = closestPt[2];
      return true;
    }

    return false;
    
   /* if(isnan(closestPt[2])){
        closestPt = queryPt;
        closestNormal<< 0,0,1;
    }*/
    
//    return closestPt[2];

  }

  inline static double get_shortest_distance_between_stickyfeet_and_markers (void *user,Eigen::Vector3f &from,Eigen::Vector3f &to)
  {
    RendererStickyFeet *self = (RendererStickyFeet*) user;
    collision::Collision_Object * intersected_object = NULL;
    Eigen::Vector3f hit_pt;
    double shortest_distance = -1;
    
    if(self->footStepPlanListener->_gl_in_motion_copy) // exists
    {
      if(self->footStepPlanListener->_gl_in_motion_copy->is_bodypose_adjustment_enabled())
      {
    
          self->footStepPlanListener->_gl_in_motion_copy->_collision_detector_floatingbase_markers->ray_test( from, to, intersected_object,hit_pt);
        
          if(intersected_object != NULL )
          {
              self->ray_hit = hit_pt;
              self->ray_hit_t = (hit_pt - self->ray_start).norm();
              Eigen::Vector3f diff = (from-hit_pt);
              double distance = diff.norm();
              if(shortest_distance>0) {
                if (distance < shortest_distance)
                {
                  shortest_distance = distance;
                  self->ray_hit = hit_pt;
                  self->ray_hit_drag = hit_pt;
                  self->ray_hit_t = (hit_pt - self->ray_start).norm();
                  //(*self->selection)  =  ;
                  (*self->marker_selection)  = string(intersected_object->id().c_str());
                }
              }
              else {
                shortest_distance = distance;
                self->ray_hit = hit_pt;
                self->ray_hit_drag = hit_pt;
                self->ray_hit_t = (hit_pt - self->ray_start).norm();
                //(*self->selection)  =  ;
                (*self->marker_selection)  = string(intersected_object->id().c_str());
               }
          }
          else 
          {
            // clear previous selections
            string no_selection = " ";
            self->footStepPlanListener->_gl_in_motion_copy->highlight_link(no_selection); 
          }  // end if-else intersected_object !=NULL;
                     
      }
    } 
  
    // give preference to markers. if shortest distance was not set previous only then check if collisions to sticky feet occurs.
    if(shortest_distance == -1)
    {
    
      // start with i=2 so that the first two sticky feet (which show the current robot feet poses) aren't selectable
      for(uint i = 2; i < self->footStepPlanListener->_gl_planned_stickyfeet_list.size(); i++) 
      { 

       self->footStepPlanListener->_gl_planned_stickyfeet_list[i]->_collision_detector->ray_test( from, to, intersected_object,hit_pt );
        if( intersected_object != NULL )
        {
          Eigen::Vector3f diff = (from-hit_pt);
          double distance = diff.norm();
          if(shortest_distance>0) {
            if (distance < shortest_distance)
            {
              shortest_distance = distance;
              self->ray_hit = hit_pt;
              self->ray_hit_drag = hit_pt;
              self->ray_hit_t = (hit_pt - self->ray_start).norm();
              self->selected_planned_footstep_index = i;
              (*self->marker_selection)  = " ";
            }
          }
          else {
            shortest_distance = distance;
            self->ray_hit = hit_pt;
            self->ray_hit_drag = hit_pt;
            self->ray_hit_t = (hit_pt - self->ray_start).norm();
            self->selected_planned_footstep_index=i;
            (*self->marker_selection)  = " ";
          }
          intersected_object = NULL; 
        }
        else 
        {
          // clear previous selections
           string no_selection = " ";
           self->footStepPlanListener->_gl_planned_stickyfeet_list[i]->highlight_link(no_selection); 
        }   // end if-else intersected_object !=NULL;
        
      }//end for 
    
    }  //end if(shortest_distance == -1)

    self->prev_ray_hit_drag = self->ray_hit_drag;                   

    return shortest_distance;  
  }


// ===================================================================

  inline static void publish_footstep_plan_constraint(void *user,string &channel, int index)
  {
    RendererStickyFeet *self = (RendererStickyFeet*) user;

    //drc::traj_opt_constraint_t msg;
    drc::footstep_plan_t msg;
    drc::footstep_goal_t goal_msg;
    msg.robot_name =  self->footStepPlanListener->_robot_name;    

    int64_t utime = self->robot_utime; // usually this should come from the ROBOT_UTIME subscription, which is broadcast from ros2lcm translator via gazebo.
    utime = self->footStepPlanListener->_last_plan_msg_timestamp; // just for debugging use this; TODO: remove later
    msg.utime = utime;
    goal_msg.utime = utime;
    
    goal_msg.robot_name = msg.robot_name;
    msg.num_steps = 1;
  
    KDL::Frame T_worldframe_footframe = self->footStepPlanListener->_gl_in_motion_copy->_T_world_body;
    // KDL::Frame T_worldframe_meshframe;
    
    size_t i= (size_t)index;
    goal_msg.is_right_foot = (self->footStepPlanListener->_planned_stickyfeet_info_list[i].foot_type== FootStepPlanListener::RIGHT);
    
    // if(!goal_msg.is_right_foot)
    // {
    //   KDL::Frame T_groundframe_bodyframe=self->footStepPlanListener->_T_bodyframe_groundframe_left.Inverse();
    //   KDL::Frame T_bodyframe_meshframe=self->footStepPlanListener->_T_bodyframe_meshframe_left;
    //   T_worldframe_meshframe =  T_worldframe_groundframe*T_groundframe_bodyframe*T_bodyframe_meshframe;
    // }
    // else
    // {
    //   KDL::Frame T_groundframe_bodyframe=self->footStepPlanListener->_T_bodyframe_groundframe_right.Inverse();
    //   KDL::Frame T_bodyframe_meshframe=self->footStepPlanListener->_T_bodyframe_meshframe_right;
    //   T_worldframe_meshframe =  T_worldframe_groundframe*T_groundframe_bodyframe*T_bodyframe_meshframe;
    // } 
    
    drc::position_3d_t pose;
    transformKDLToLCM(T_worldframe_footframe,pose); 

     goal_msg.pos = pose;
     // goal_msg.step_time = 0.0; // Ignored on the other end
     goal_msg.step_speed = self->footStepPlanListener->_planned_stickyfeet_info_list[i].step_speed;
     goal_msg.step_height = self->footStepPlanListener->_planned_stickyfeet_info_list[i].step_height;
     goal_msg.id = self->footStepPlanListener->_gl_planned_stickyfeet_ids[i];
     goal_msg.fixed_x = 1;
     goal_msg.fixed_y = 1;
     goal_msg.fixed_z = 1;
     goal_msg.fixed_roll = 1;
     goal_msg.fixed_pitch = 1;
     goal_msg.fixed_yaw = 1;
     goal_msg.num_terrain_pts = 0;
     msg.footstep_goals.push_back(goal_msg);

     self->lcm->publish(channel, &msg);

  }
  

// ===================================================================
 
  inline static void set_object_desired_state_on_marker_motion(void *user)
  {
      RendererStickyFeet *self = (RendererStickyFeet*) user;
      
      int index = self->footStepPlanListener->get_motion_copy_index();// gets the in motion copy's index, index can changes as plan is updated.  it no motion copy exists it returns -1, this should never happen.
      if(index==-1){
         cerr << "ERROR: set_object_desired_state_on_marker_motion in sticky feet renderer called but no in_motion_copy exists"<< endl;
         return;
      } 

      double gain = 1;      
      // set desired state
      KDL::Frame T_world_object = self->footStepPlanListener->_gl_in_motion_copy->_T_world_body;
      double currentAngle, angleTo,dtheta;       
      KDL::Frame DragRotation=KDL::Frame::Identity();       

      //cout << (*self->marker_selection) << endl;
      
      
      std::string token  = "plane::";
      size_t found = (*self->marker_selection).find(token);  
      if(found!=std::string::npos)  
      {
        string plane_name="";
        string root_link_name=self->footStepPlanListener->_gl_in_motion_copy->get_root_link_name();
        self->footStepPlanListener->_gl_in_motion_copy->extract_plane_name(root_link_name,plane_name);
        size_t found2 = plane_name.find("x"); 
        bool x_plane_active = (found2!=std::string::npos);
        found2 = plane_name.find("y"); 
        bool y_plane_active = (found2!=std::string::npos);       
        found2 = plane_name.find("z"); 
        bool z_plane_active = (found2!=std::string::npos);
        if(x_plane_active){
         double dx =  self->ray_hit_drag[0]-self->marker_offset_on_press[0];
         T_world_object.p[0] = dx;
        }
        if(y_plane_active){
         double dy =  self->ray_hit_drag[1]-self->marker_offset_on_press[1];
          T_world_object.p[1] = dy;
        }
        if(z_plane_active){
          double dz =  self->ray_hit_drag[2]-self->marker_offset_on_press[2];
          T_world_object.p[2] = dz;
        }        
      }  
      
      if((*self->marker_selection)=="markers::base_x"){
        double dx =  self->ray_hit_drag[0]-self->marker_offset_on_press[0];
        T_world_object.p[0] = dx;
      }
      else if((*self->marker_selection)=="markers::base_y"){
        double dy =  self->ray_hit_drag[1]-self->marker_offset_on_press[1];
        T_world_object.p[1] = dy;
      }
      else if((*self->marker_selection)=="markers::base_z"){
        double dz =  self->ray_hit_drag[2]-self->marker_offset_on_press[2];
        T_world_object.p[2] = dz;
      }
      else if((*self->marker_selection)=="markers::base_roll"){
        currentAngle = atan2(self->prev_ray_hit_drag[2]-T_world_object.p[2],self->prev_ray_hit_drag[1]-T_world_object.p[1]);
        angleTo = atan2(self->ray_hit_drag[2]-T_world_object.p[2],self->ray_hit_drag[1]-T_world_object.p[1]);
        dtheta = gain*shortest_angular_distance(currentAngle,angleTo);
        //dtheta =  atan2(sin(angleTo - currentAngle), cos(angleTo - currentAngle));
        KDL::Vector axis;
        axis[0] = 1; axis[1] = 0; axis[2]=0;
        DragRotation.M = KDL::Rotation::Rot(axis,dtheta);
      }
      else if((*self->marker_selection)=="markers::base_pitch"){ 
        currentAngle = atan2(self->prev_ray_hit_drag[0]-T_world_object.p[0],self->prev_ray_hit_drag[2]-T_world_object.p[2]);
        angleTo = atan2(self->ray_hit_drag[0]-T_world_object.p[0],self->ray_hit_drag[2]-T_world_object.p[2]);
        dtheta = gain*shortest_angular_distance(currentAngle,angleTo);
        //dtheta =  atan2(sin(angleTo - currentAngle), cos(angleTo - currentAngle));
        KDL::Vector axis;
        axis[0] = 0; axis[1] = 1; axis[2]=0;
        DragRotation.M = KDL::Rotation::Rot(axis,dtheta);
      }    
      else if((*self->marker_selection)=="markers::base_yaw"){
        currentAngle = atan2(self->prev_ray_hit_drag[1]-T_world_object.p[1],self->prev_ray_hit_drag[0]-T_world_object.p[0]);
        angleTo = atan2(self->ray_hit_drag[1]-T_world_object.p[1],self->ray_hit_drag[0]-T_world_object.p[0]);
        dtheta = gain*shortest_angular_distance(currentAngle,angleTo);
        KDL::Vector axis;
        axis[0] = 0; axis[1] = 0; axis[2]=1;
        DragRotation.M = KDL::Rotation::Rot(axis,dtheta);
      }
 
 
      T_world_object.M  = DragRotation.M*T_world_object.M;  
      
      std::map<std::string, double> jointpos_in;
      jointpos_in = self->footStepPlanListener->_gl_in_motion_copy->_current_jointpos;
      self->footStepPlanListener->_gl_in_motion_copy->set_state(T_world_object,jointpos_in); 
      

      self->prev_ray_hit_drag = self->ray_hit_drag;
      
      string channel = "FOOTSTEP_PLAN_CONSTRAINT";
      publish_footstep_plan_constraint(self,channel,index);

  }   // end set_object_desired_state_on_marker_motion()   

}// end namespace


void setup_renderer_sticky_feet(BotViewer *viewer, int render_priority, lcm_t *lcm, BotParam * param,
    BotFrames * frames, int operation_mode);

#endif //RENDERER_STICKYFEET_HPP

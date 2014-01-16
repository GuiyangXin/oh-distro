#include "renderer_affordances.hpp"
#include "AffordanceCollectionListener.hpp"
#include "RobotStateListener.hpp"
#include "InitGraspOptPublisher.hpp"
#include "CandidateGraspSeedListener.hpp"
#include "GraspOptStatusListener.hpp"

#include "ReachabilityVerifier.hpp"
#include "otdf_instance_management_gui_utils.hpp"
#include "object_interaction_gui_utils.hpp"
#include "stickyhand_interaction_gui_utils.hpp"
#include "stickyfoot_interaction_gui_utils.hpp"
#include "mixedseed_interaction_gui_utils.hpp"
#include "lcm_utils.hpp"

#define GEOM_EPSILON 1e-9
#define PARAM_COLOR_ALPHA "Alpha"
#define PARAM_DEBUG_MODE "Local Aff Store (Debug Only)"
#define PARAM_GRASP_OPT_MODE "GraspOpt Mode"

using namespace std;
using namespace boost;
using namespace visualization_utils;
using namespace collision;
using namespace renderer_affordances;
using namespace renderer_affordances_gui_utils;

// =================================================================================
// DRAWING

// TODO: Super bloated need to break it up later.
static void _draw (BotViewer *viewer, BotRenderer *renderer) 
{
    RendererAffordances *self = (RendererAffordances*) renderer;

    glEnable(GL_DEPTH_TEST);

    //-draw 
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_BLEND);
    // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); 
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable (GL_RESCALE_NORMAL);
    glEnable(GL_NORMALIZE); // this is required to renderer cubes/boxes, otherwise certain faces will be blank 

  


    if((self->ehandler.picking)&&(self->selection_enabled)&&(self->clicked)){
        glLineWidth (3.0);
        glPushMatrix();
        glBegin(GL_LINES);
        glVertex3f(self->ray_start[0], self->ray_start[1],self->ray_start[2]); // object coord
        glVertex3f(self->ray_end[0], self->ray_end[1],self->ray_end[2]);
        glEnd();
        glPopMatrix();
        
        if((self->dragging)&&(self->marker_selection==" ")) {
            Eigen::Vector3f diff = self->ray_hit_drag - self->ray_hit;
            double length =diff.norm();
            double head_width = 0.03; double head_length = 0.03;double body_width = 0.01;
            glColor4f(0,0,0,1);
            glPushMatrix();
            glTranslatef(self->ray_hit[0], self->ray_hit[1],self->ray_hit[2]);
            //--get rotation in angle/axis form
            double theta;
            Eigen::Vector3f axis;// = self->ray_hit-self->ray_start;
            diff.normalize();
            Eigen::Vector3f uz,ux;   uz << 0 , 0 , 1;ux << 1 , 0 , 0;
            axis = ux.cross(diff);axis.normalize();
            theta = acos(ux.dot(diff));
            
            glRotatef(theta * 180/3.141592654, axis[0], axis[1], axis[2]); 
            glTranslatef(length/2, 0,0);
            bot_gl_draw_arrow_3d(length,head_width, head_length,body_width);
            glPopMatrix();
        }
        
    }
  
    float c[3] = {0.3,0.3,0.6};
    //float alpha = 0.8;

    // Draw all OTDF objectes.
    typedef map<string, OtdfInstanceStruc > object_instance_map_type_;
    for(object_instance_map_type_::const_iterator it = self->affCollection->_objects.begin(); it!=self->affCollection->_objects.end(); it++) {
        
        // draw object   
        double pos[3];
        pos[0] = it->second._gl_object->_T_world_body.p[0]; 
        pos[1] = it->second._gl_object->_T_world_body.p[1]; 
        pos[2] = it->second._gl_object->_T_world_body.p[2]+0.0;  
        std::stringstream oss;
        oss << it->second.uid;
        glColor4f(0,0,0,1);
        bot_gl_draw_text(pos, GLUT_BITMAP_HELVETICA_18, (oss.str()).c_str(),0);
        it->second._gl_object->enable_link_selection(self->selection_enabled);
        it->second._gl_object->isShowMeshSelected = self->showMesh; 
        it->second._gl_object->draw_body(c,self->alpha); // no using sliders
        
        // get object transformation from KDF::Frame
        KDL::Frame& nextTfframe = it->second._gl_object->_T_world_body;
        double theta;
        double axis[3];
        double x,y,z,w;
        nextTfframe.M.GetQuaternion(x,y,z,w);
        double quat[4] = {w,x,y,z};
        bot_quat_to_angle_axis(quat, &theta, axis);
        
        // draw bounding box
        if(self->showBoundingBox){
            glPushMatrix();
            glColor4f(c[0],c[1],c[2],self->alpha);
            // apply base transform
            glTranslatef(nextTfframe.p[0], nextTfframe.p[1], nextTfframe.p[2]);
            glRotatef(theta * 180/M_PI, axis[0], axis[1], axis[2]); 
            
            // apply bounding box translate
            const Eigen::Vector3f& bbXYZ = it->second.boundingBoxXYZ;
            glTranslatef(bbXYZ[0],bbXYZ[1],bbXYZ[2]);
            
            // apply bounding box rotation
            const Eigen::Vector3f& bbRPY = it->second.boundingBoxRPY;
            double bbRPYArr[] = {bbRPY[0],bbRPY[1],bbRPY[2]};
            double bbQuat[4], bbTheta, bbAxis[3];
            bot_roll_pitch_yaw_to_quat(bbRPYArr,bbQuat);
            bot_quat_to_angle_axis(bbQuat, &bbTheta, bbAxis);
            glRotatef(bbTheta * 180/M_PI, bbAxis[0], bbAxis[1], bbAxis[2]); 
            
            // Draw box
            const Eigen::Vector3f& lwh = it->second.boundingBoxLWH;
            if(lwh[0]>0 && lwh[1]>0 && lwh[2]>0){
                glScalef(lwh[0],lwh[1],lwh[2]);
                glutWireCube(1.0);
            } 
            glPopMatrix();
        }
        
        if(self->showTriad){
            double size = it->second.boundingBoxLWH.maxCoeff()*.55;  // make size proportional to bounding box
            if(size==0) size=1;
            double rpy[3];
            bot_quat_to_roll_pitch_yaw(quat,rpy);
            draw_axis(pos[0],pos[1],pos[2],rpy[2],rpy[1],rpy[0],size,false);
        }
    }
    
    if(self->selection_hold_on) { 
        //draw temp object
        self->otdf_instance_hold._gl_object->draw_body(c,0.3);
    }
    
    // Draw all sticky hands
    float c_green[3] = {0.3,0.5,0.3}; 
    float c_yellow[3] = {0.5,0.5,0.3};
    float c_gray[3] = {0.3,0.3,0.3};
    float alpha = 0.6;
    typedef map<string, StickyHandStruc > sticky_hands_map_type_;
    for(sticky_hands_map_type_::const_iterator hand_it = self->stickyHandCollection->_hands.begin(); hand_it!=self->stickyHandCollection->_hands.end(); hand_it++) {
        hand_it->second._gl_hand->enable_link_selection(self->selection_enabled);
        typedef map<string, OtdfInstanceStruc > object_instance_map_type_;
        object_instance_map_type_::iterator obj_it = self->affCollection->_objects.find(string(hand_it->second.object_name));
        KDL::Frame T_world_graspgeometry = KDL::Frame::Identity(); // the object might have moved.
        
        if(!obj_it->second._gl_object->get_link_geometry_frame(string(hand_it->second.geometry_name),T_world_graspgeometry))
            cerr << " failed to retrieve " << hand_it->second.geometry_name<<" in object " << hand_it->second.object_name <<endl;
        else {  
            double r,p,y;
            T_world_graspgeometry.M.GetRPY(r,p,y);
            float ch[3];

       
            // Check reachability w.r.t to current pelvis
            bool reachable;
            if(hand_it->second.hand_type == drc::desired_grasp_state_t::SANDIA_RIGHT) {
                
                reachable = true;
                
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 
                
                if(!hand_it->second._gl_hand->get_link_frame("right_palm",T_geometry_palm))
                    cout <<"ERROR: ee link "<< "right_palm" << " not found in sticky hand urdf"<< endl;
                
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm; // but this is palm or frame
                KDL::Frame T_hand_palm_r = KDL::Frame::Identity();
                T_hand_palm_r.p[1] = -0.1;
                T_hand_palm_r.M=KDL::Rotation::RPY(-1.57079,0,-1.57079);
                
                
                KDL::Frame T_world_hand_r=T_world_palm*T_hand_palm_r.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter)){
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_r);
                }
                if(reachable){
                    ch[0]=c_green[0]; ch[1]=c_green[1];  ch[2]=c_green[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }
            else if(hand_it->second.hand_type == drc::desired_grasp_state_t::SANDIA_LEFT) {
                reachable = true;
                
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 
                if(!hand_it->second._gl_hand->get_link_frame("left_palm",T_geometry_palm))
                    cout <<"ERROR: ee link "<< "left_palm" << " not found in sticky hand urdf"<< endl;
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm;// but this is palm or frame
                
                KDL::Frame T_hand_palm_l=KDL::Frame::Identity();
                T_hand_palm_l.p[1] = 0.1;
                T_hand_palm_l.M=KDL::Rotation::RPY(1.57079,0,1.57079);
                
                KDL::Frame T_world_hand_l=T_world_palm*T_hand_palm_l.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_l);
                if(reachable){
                    ch[0]=c_yellow[0]; ch[1]=c_yellow[1];  ch[2]=c_yellow[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
            else if(hand_it->second.hand_type == drc::desired_grasp_state_t::IROBOT_RIGHT) {
                
                reachable = true;
                
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 
                
                if(!hand_it->second._gl_hand->get_link_frame("right_base_link",T_geometry_palm))
                    cout <<"ERROR: ee link "<< "right_base_link" << " not found in sticky hand urdf"<< endl;
                
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm; // but this is palm or frame
                KDL::Frame T_hand_palm_r = KDL::Frame::Identity();
                T_hand_palm_r.p[1] = -0.05;
                T_hand_palm_r.M=KDL::Rotation::RPY(1.57079, 0, 3.14159);
                
                
                KDL::Frame T_world_hand_r=T_world_palm*T_hand_palm_r.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter)){
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_r);
                }
                if(reachable){
                    ch[0]=c_green[0]; ch[1]=c_green[1];  ch[2]=c_green[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
            else if(hand_it->second.hand_type == drc::desired_grasp_state_t::IROBOT_LEFT) {
                reachable = true;
                
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 
                if(!hand_it->second._gl_hand->get_link_frame("left_base_link",T_geometry_palm))
                    cout <<"ERROR: ee link "<< "left_base_link" << " not found in sticky hand urdf"<< endl;
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm;// but this is palm or frame
                
                KDL::Frame T_hand_palm_l=KDL::Frame::Identity();
                T_hand_palm_l.p[1] = 0.05;
                T_hand_palm_l.M=KDL::Rotation::RPY(1.57079,0,1.57079);
                
                KDL::Frame T_world_hand_l=T_world_palm*T_hand_palm_l.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_l);
                if(reachable){
   
                    ch[0]=c_yellow[0]; ch[1]=c_yellow[1];  ch[2]=c_yellow[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
            hand_it->second._gl_hand->draw_body_in_frame (ch,alpha,T_world_graspgeometry);//draws in grasp_geometry frame
            string hand_name = hand_it->first;
            int order = self->seedSelectionManager->get_selection_order(hand_name);
            if((order>0)&&(self->seedSelectionManager->get_selection_cnt()>1))
            {
            
                KDL::Frame T_world_hand =  T_world_graspgeometry*hand_it->second._gl_hand->_T_world_body;
                double pos[3];
                pos[0] = T_world_hand.p[0]; 
                pos[1] = T_world_hand.p[1]; 
                pos[2] = T_world_hand.p[2];  
                std::stringstream oss;
                oss << order;
                glColor4f(0,0,1,1);
                bot_gl_draw_text(pos, GLUT_BITMAP_HELVETICA_18, (oss.str()).c_str(),0);             
            }
        }

        double alpha2 = 0.3;
        if(obj_it->second._gl_object->get_link_geometry_future_frame(string(hand_it->second.geometry_name),T_world_graspgeometry)) { // if future frame exists draw 
            double r,p,y;
            T_world_graspgeometry.M.GetRPY(r,p,y);
        
            if((obj_it->second._gl_object->is_future_state_changing())&&(self->motion_trail_log_enabled))
                hand_it->second._gl_hand->log_motion_trail(true);
            else {
                hand_it->second._gl_hand->log_motion_trail(false);
            }
          
          
            float ch[3];//SANDIA_LEFT=0, SANDIA_RIGHT=1,

            // Check reachability w.r.t to current pelvis
            bool reachable;
            if(hand_it->second.hand_type == drc::desired_grasp_state_t::SANDIA_RIGHT)
            {
                reachable = true;
      
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 

                hand_it->second._gl_hand->get_link_future_frame("right_palm",T_geometry_palm);      
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm; // but this is palm or frame
                KDL::Frame T_hand_palm_r = KDL::Frame::Identity();
                T_hand_palm_r.p[1] = -0.145;
                T_hand_palm_r.M=KDL::Rotation::RPY(-1.57079,0,-1.57079);


                KDL::Frame T_world_hand_r=T_world_palm*T_hand_palm_r.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter)){
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_r);
                }
                if(reachable){
                    ch[0]=c_green[0]; ch[1]=c_green[1];  ch[2]=c_green[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
      
            else if(hand_it->second.hand_type == drc::desired_grasp_state_t::SANDIA_LEFT)
            {
                reachable = true;
      
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 
                hand_it->second._gl_hand->get_link_future_frame("left_palm",T_geometry_palm);
       
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm;// but this is palm or frame
      
                KDL::Frame T_hand_palm_l=KDL::Frame::Identity();
                T_hand_palm_l.p[1] = 0.145;
                T_hand_palm_l.M=KDL::Rotation::RPY(1.57079,0,1.57079);

                KDL::Frame T_world_hand_l=T_world_palm*T_hand_palm_l.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_l);
                if(reachable){
                    ch[0]=c_yellow[0]; ch[1]=c_yellow[1];  ch[2]=c_yellow[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
            else if(hand_it->second.hand_type == drc::desired_grasp_state_t::IROBOT_RIGHT)
            {
                reachable = true;
      
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 

                hand_it->second._gl_hand->get_link_future_frame("right_base_link",T_geometry_palm);      
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm; // but this is palm or frame
                KDL::Frame T_hand_palm_r = KDL::Frame::Identity();
                T_hand_palm_r.p[1] = -0.095;
                T_hand_palm_r.M=KDL::Rotation::RPY(1.57079, 0, 3.14159);

                KDL::Frame T_world_hand_r=T_world_palm*T_hand_palm_r.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter)){
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_r);
                }
                if(reachable){
                    ch[0]=c_green[0]; ch[1]=c_green[1];  ch[2]=c_green[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            } 
            else if(hand_it->second.hand_type == drc::desired_grasp_state_t::IROBOT_LEFT)
            {
                reachable = true;
      
                KDL::Frame  T_geometry_palm = KDL::Frame::Identity(); 
                hand_it->second._gl_hand->get_link_future_frame("left_base_link",T_geometry_palm);
       
                KDL::Frame T_world_palm = T_world_graspgeometry*T_geometry_palm;// but this is palm or frame
      
                KDL::Frame T_hand_palm_l=KDL::Frame::Identity();
                T_hand_palm_l.p[1] = 0.095;
                T_hand_palm_l.M=KDL::Rotation::RPY(1.57079,0,1.57079);

                KDL::Frame T_world_hand_l=T_world_palm*T_hand_palm_l.Inverse();
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_hand(self->robotStateListener->_last_robotstate_msg,hand_it->second.hand_type,T_world_hand_l);
                if(reachable){
                    ch[0]=c_yellow[0]; ch[1]=c_yellow[1];  ch[2]=c_yellow[2];
                }
                else{
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
              
            hand_it->second._gl_hand->draw_body_in_frame (ch,alpha2,T_world_graspgeometry);
        
            KDL::Frame T_world_object = obj_it->second._gl_object->_T_world_body;
            KDL::Frame T_object_graspgeometry = T_world_object.Inverse()*T_world_graspgeometry;
            if(obj_it->second._gl_object->is_future_display_active())
                hand_it->second._gl_hand->accumulate_and_draw_motion_trail(c_gray,0.8,T_world_object,T_object_graspgeometry); // accumulates in object frame.
            else{    
                hand_it->second._gl_hand->disable_future_display();
            }

        } 
    }
  
    // Draw all sticky feet 
    typedef map<string, StickyFootStruc > sticky_feet_map_type_;
    for(sticky_feet_map_type_::const_iterator foot_it = self->stickyFootCollection->_feet.begin(); foot_it!=self->stickyFootCollection->_feet.end(); foot_it++) {
        foot_it->second._gl_foot->enable_link_selection(self->selection_enabled);
        typedef map<string, OtdfInstanceStruc > object_instance_map_type_;
        object_instance_map_type_::iterator obj_it = self->affCollection->_objects.find(string(foot_it->second.object_name));
        KDL::Frame T_world_geometry = KDL::Frame::Identity(); // the object might have moved.
        if(!obj_it->second._gl_object->get_link_geometry_frame(string(foot_it->second.geometry_name),T_world_geometry)){
            cerr << " failed to retrieve " << string(foot_it->second.geometry_name)<<" in object " << string(foot_it->second.object_name) <<endl;
        }
        else {  
            double r,p,y;
            T_world_geometry.M.GetRPY(r,p,y);
            float ch[3];
          
          
            // Check reachability w.r.t to current pelvis
            bool reachable;
            if(foot_it->second.foot_type == 1) {//SANDIA_LEFT=0, SANDIA_RIGHT=1,

                reachable = true;
                
                KDL::Frame  T_geometry_foot = KDL::Frame::Identity(); 
                
                if(!foot_it->second._gl_foot->get_link_frame("r_foot",T_geometry_foot))
                    cout <<"ERROR: ee link "<< "r_foot" << " not found in sticky foot urdf"<< endl;
                
                KDL::Frame T_world_foot_r = T_world_geometry*T_geometry_foot; // but this is palm or frame
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_foot(self->robotStateListener->_last_robotstate_msg,foot_it->second.foot_type,T_world_foot_r);
                if(reachable) {
                    ch[0]=c_green[0]; ch[1]=c_green[1];  ch[2]=c_green[2];
                } else {
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
            
            if(foot_it->second.foot_type == 0) {
                reachable = true;
            
                KDL::Frame  T_geometry_foot = KDL::Frame::Identity(); 
                if(!foot_it->second._gl_foot->get_link_frame("l_foot",T_geometry_foot))
                    cout <<"ERROR: ee link "<< "l_foot" << " not found in sticky foot urdf"<< endl;
                KDL::Frame T_world_foot_l = T_world_geometry*T_geometry_foot;// but this is palm or frame
            
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_foot(self->robotStateListener->_last_robotstate_msg,foot_it->second.foot_type,T_world_foot_l);
                if(reachable) {
                    ch[0]=c_yellow[0]; ch[1]=c_yellow[1];  ch[2]=c_yellow[2];
                } else {
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
        
            foot_it->second._gl_foot->draw_body_in_frame (ch,alpha,T_world_geometry);//draws in geometry frame
            string foot_name = foot_it->first;
            int order = self->seedSelectionManager->get_selection_order(foot_name);
            if((order>0)&&(self->seedSelectionManager->get_selection_cnt()>1))
            {
            
                KDL::Frame T_world_foot =  T_world_geometry*foot_it->second._gl_foot->_T_world_body;
                double pos[3];
                pos[0] = T_world_foot.p[0]; 
                pos[1] = T_world_foot.p[1]; 
                pos[2] = T_world_foot.p[2];  
                std::stringstream oss;
                oss << order;
                glColor4f(0,0,1,1);
                bot_gl_draw_text(pos, GLUT_BITMAP_HELVETICA_18, (oss.str()).c_str(),0);             
            }
        }
    
        double alpha2 =  0.3;
        if(obj_it->second._gl_object->get_link_geometry_future_frame(foot_it->second.geometry_name,T_world_geometry)) { // if future frame exists draw 
            double r,p,y;
            T_world_geometry.M.GetRPY(r,p,y);
        
            if((obj_it->second._gl_object->is_future_state_changing())&&(self->motion_trail_log_enabled)) // logging is not enabled when setting range goals or manual motion goals 
                foot_it->second._gl_foot->log_motion_trail(true); //  enables motion trail logging and motion trail display too
            else{
                foot_it->second._gl_foot->log_motion_trail(false);
            }
          
            float ch[3];//SANDIA_LEFT=0, SANDIA_RIGHT=1,
            /*ch[0]=c_green[0]; ch[1]=c_green[1];  ch[2]=c_green[2];
              if(foot_it->second.foot_type == 0) {
              ch[0]=c_yellow[0]; ch[1]=c_yellow[1];  ch[2]=c_yellow[2];
              }*/
            // Check reachability w.r.t to current pelvis
            bool reachable;
            if(foot_it->second.foot_type == 1) {
                reachable = true;
            
                KDL::Frame  T_geometry_foot = KDL::Frame::Identity(); 
            
                foot_it->second._gl_foot->get_link_future_frame("r_foot",T_geometry_foot);
            
                KDL::Frame T_world_foot_r = T_world_geometry*T_geometry_foot; // but this is palm or frame
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_foot(self->robotStateListener->_last_robotstate_msg,foot_it->second.foot_type,T_world_foot_r);
            
                if(reachable) {
                    ch[0]=c_green[0]; ch[1]=c_green[1];  ch[2]=c_green[2];
                } else {
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            }   
        
            if(foot_it->second.foot_type == 0) {
                reachable = true;
            
                KDL::Frame  T_geometry_foot = KDL::Frame::Identity(); 
                foot_it->second._gl_foot->get_link_future_frame("l_foot",T_geometry_foot);
                KDL::Frame T_world_foot_l = T_world_geometry*T_geometry_foot;// but this is palm or frame
            
                if((self->robotStateListener->_robot_state_received)&&(self->enableReachabilityFilter))
                    reachable = self->reachabilityVerifier->has_IK_solution_from_pelvis_to_foot(self->robotStateListener->_last_robotstate_msg,foot_it->second.foot_type,T_world_foot_l);
                if(reachable) {
                    ch[0]=c_yellow[0]; ch[1]=c_yellow[1];  ch[2]=c_yellow[2];
                } else {
                    ch[0]=c_gray[0]; ch[1]=c_gray[1];  ch[2]=c_gray[2];
                }
            
            }  
            foot_it->second._gl_foot->draw_body_in_frame (ch,alpha2,T_world_geometry);
        
            KDL::Frame T_world_object = obj_it->second._gl_object->_T_world_body;
            KDL::Frame T_object_geometry = T_world_object.Inverse()*T_world_geometry;
            if(obj_it->second._gl_object->is_future_display_active()) {
                if(foot_it->second._gl_foot->is_motion_trail_log_active())
                    foot_it->second._gl_foot->accumulate_and_draw_motion_trail (c_gray,0.8,T_world_object,T_object_geometry); // accumulates in object frame.
                else
                    foot_it->second._gl_foot->draw_motion_trail(c_gray,0.8,T_world_object);
            }
            else
                foot_it->second._gl_foot->disable_future_display();//also clears motion history if it exists.
        }
    }
  
    // Draw the manip map end effector pose
    if (bot_gtk_param_widget_get_bool (self->pw, PARAM_SHOW_PROPOSED_MANIP_MAP) && 
        !self->ee_frames_map.empty()) {
      
        // N ee's and K keyframes
        for(map<string,vector<KDL::Frame> >::iterator it = self->ee_frames_map.begin(); it != self->ee_frames_map.end(); it++) { 
            string ee_name = it->first;
            vector<KDL::Frame> ee_frames  = it->second;
            map<string, vector<drc::affordance_index_t> >::iterator ts_it = self->ee_frame_affindices_map.find(it->first);
            if(ts_it == self->ee_frame_affindices_map.end()){
                cerr << "ERROR: No Aff index found for ee " << it->first << endl;      
                continue;
            }

            vector<drc::affordance_index_t> ee_frame_affindices = ts_it->second;
            glLineWidth (10.0);
            glBegin(GL_LINES);
            for(uint i = 0; i < ((uint) ee_frames.size()-1); i++) {   
                KDL::Frame T_world_ee_1 = ee_frames[i];
                KDL::Frame T_world_ee_2 = ee_frames[i+1];
              
                drc::affordance_index_t aff_index = ee_frame_affindices[i];
                double aff_value = aff_index.dof_value[0];
              
                glVertex3f (T_world_ee_1.p[0], T_world_ee_1.p[1], T_world_ee_1.p[2]);
                glVertex3f (T_world_ee_2.p[0], T_world_ee_2.p[1], T_world_ee_2.p[2]);
            } // end for frames
            glEnd();

            for(uint i = 0; i < (uint) ee_frames.size(); i++) {   
                KDL::Frame T_world_ee = ee_frames[i];
                drc::affordance_index_t aff_index = ee_frame_affindices[i];
                double aff_value = aff_index.dof_value[0];

                double pos[] = {T_world_ee.p[0], T_world_ee.p[1], T_world_ee.p[2] + 0.2};
                char line[256];
                sprintf (line, "%.2f", aff_value);
                glColor3f(1.0, 0.0, 0.0);
                bot_gl_draw_text (pos, GLUT_BITMAP_HELVETICA_12, line, 0);
            } // end for frames
        }
    }

    // Maintain heartbeat with Drake
    int64_t now = bot_timestamp_now();
    if(now > (self->graspOptStatusListener->_last_statusmsg_stamp + 4000000)){
        bot_gtk_param_widget_set_bool(self->pw,PARAM_OPT_POOL_READY,false); 
    }
  

}
// =================================================================================
// EVENT HANDLING
// ----------------------------------------------------------------------------
static double pick_query (BotViewer *viewer, BotEventHandler *ehandler, const double ray_start[3], const double ray_dir[3])
{
    RendererAffordances *self = (RendererAffordances*) ehandler->user;
  
    
    if(self->dblclk_popup){   
        fprintf(stderr, "Object DblClk Popup is Open. Closing \n");
        gtk_widget_destroy(self->dblclk_popup);
    }
  
    if((self->second_stage_popup)){   
        fprintf(stderr, "second_stage_popup is Open. Closing \n");
        gtk_widget_destroy(self->second_stage_popup);
    }
  
    if(self->selection_enabled==0){
        return -1.0;
    }
    Eigen::Vector3f from,to;
    from << ray_start[0], ray_start[1], ray_start[2];

    Eigen::Vector3f plane_normal,plane_pt;
    plane_normal << 0,0,1;
    if(ray_start[2]<0)
        plane_pt << 0,0,10;
    else
        plane_pt << 0,0,-10;
 
   
    double lambda1 = ray_dir[0] * plane_normal[0]+
        ray_dir[1] * plane_normal[1]+
        ray_dir[2] * plane_normal[2];
    // check for degenerate case where ray is (more or less) parallel to plane
    if (fabs (lambda1) < 1e-9) return -1.0;

    double lambda2 = (plane_pt[0] - ray_start[0]) * plane_normal[0] +
        (plane_pt[1] - ray_start[1]) * plane_normal[1] +
        (plane_pt[2] - ray_start[2]) * plane_normal[2];
    double t = fabs(lambda2 / lambda1);// =1;
  
    to << ray_start[0]+t*ray_dir[0], ray_start[1]+t*ray_dir[1], ray_start[2]+t*ray_dir[2];
    self->ray_start = from;
    self->ray_end = to;
    self->ray_hit_t = t;
    self->ray_hit_drag = to;
    self->ray_hit = to;
    self->ray_hit_normal << 0,0,1;
    double shortest_distance = get_shortest_distance_between_objects_markers_sticky_hands_and_feet(self,from,to);
    if((shortest_distance<0)&&(!self->seedSelectionManager->is_shift_pressed()))
      self->seedSelectionManager->clear();
    
    return shortest_distance;
}

// ----------------------------------------------------------------------------
static int mouse_press (BotViewer *viewer, BotEventHandler *ehandler, const double ray_start[3], const double ray_dir[3], const GdkEventButton *event)
{
    RendererAffordances *self = (RendererAffordances*) ehandler->user;
   
    //std::cout << "Aff ehandler->picking " << ehandler->picking << std::endl;
    if((ehandler->picking==0)||(self->selection_enabled==0)){     
        return 0;
    } 
    
    if(self->dblclk_popup){   
        fprintf(stderr, "Object DblClk Popup is Open. Closing \n");
        gtk_widget_destroy(self->dblclk_popup);
    }
  


    self->clicked = 1;

    if(self->stickyhand_selection!=" "){
        typedef map<string, StickyHandStruc > sticky_hands_map_type_;
        sticky_hands_map_type_::iterator hand_it = self->stickyHandCollection->_hands.find(self->stickyhand_selection);
        //hand_it->second._gl_hand->enable_whole_body_selection(true); 
        //hand_it->second._gl_hand->highlight_link(self->stickyhand_selection);
        cout << "intersected stickyhand:" << (self->stickyhand_selection) << " at: "<< self->ray_hit.transpose() << endl;
        self->seedSelectionManager->add(self->stickyhand_selection);
        self->stickyHandCollection->highlight_selected(self->seedSelectionManager);
        self->stickyFootCollection->highlight_selected(self->seedSelectionManager);
    }
    else if(self->stickyfoot_selection!=" "){
        
        typedef map<string, StickyFootStruc > sticky_feet_map_type_;
        sticky_feet_map_type_::iterator foot_it = self->stickyFootCollection->_feet.find(self->stickyfoot_selection);
        //foot_it->second._gl_foot->enable_whole_body_selection(true); 
        //foot_it->second._gl_foot->highlight_link(self->stickyfoot_selection);
        cout << "intersected stickyfoot:" << self->stickyfoot_selection << " at: "<< self->ray_hit.transpose() << endl;
        self->seedSelectionManager->add(self->stickyfoot_selection);
        self->stickyHandCollection->highlight_selected(self->seedSelectionManager);
        self->stickyFootCollection->highlight_selected(self->seedSelectionManager);
    }
    else if(self->object_selection!=" "){
      self->seedSelectionManager->clear();
       // NOTE: cannot use self->object_selection affCollection if selection hold is on;      
      object_instance_map_type_::iterator obj_it = self->affCollection->_objects.find(self->object_selection);
      if((self->marker_selection!=" ")&&(!self->selection_hold_on)) {
          obj_it->second._gl_object->highlight_marker(self->marker_selection);
          cout << "intersected an object's marker: " << self->marker_selection << " at: " << self->ray_hit.transpose() << endl;
      }
      else if((self->marker_selection!=" ")&&(self->selection_hold_on)) {
          self->otdf_instance_hold._gl_object->highlight_marker(self->marker_selection);
          cout << "intersected an temp object's marker: " << self->marker_selection << " at: " << self->ray_hit.transpose() << endl;
      }
      else {
          obj_it->second._gl_object->highlight_link(self->link_selection); 
          cout << "intersected an object's link: " << self->link_selection<< " at: " << self->ray_hit.transpose() << endl;
      }
    }

    //(event->button==3) -- Right Click
    //cout << "current selection:" << self->link_selection  <<  endl;
    if(((self->link_selection  != " ") || (self->marker_selection  != " ")) &&(event->button==1) &&(event->type==GDK_2BUTTON_PRESS))
    {
        //spawn_object_geometry_dblclk_popup(self);
        // draw circle for angle specification around the axis.
        self->dragging = 1;
        self->show_popup_onrelease = 1;
        bot_viewer_request_redraw(self->viewer);
        std::cout << "RendererAffordances: Event is consumed" <<  std::endl;

        return 1;// consumed if pop up comes up.
    }
    else if((self->stickyhand_selection != " ")&&(event->button==1)&&(event->type==GDK_2BUTTON_PRESS)){
        spawn_sticky_hand_dblclk_popup(self);
        std::cout << "RendererAffordances: Event is consumed" <<  std::endl;
        return 1;// consumed if pop up comes up.
    }
    else if((self->stickyfoot_selection  != " ")&&(event->button==1)&&(event->type==GDK_2BUTTON_PRESS)){
        spawn_sticky_foot_dblclk_popup(self);
        std::cout << "RendererAffordances: Event is consumed" <<  std::endl;
        return 1;// consumed if pop up comes up.
    }
    else if((self->marker_selection  != " "))
    {
        string token  = "markers::";
        size_t found = self->marker_selection.find(token);
        string joint_name= " ";  
        if (found!=std::string::npos)
          joint_name =self->marker_selection.substr(found+token.size());
          
        token  = "mate::";
        found = self->marker_selection.find(token);  
        KDL::Frame T_world_mate_endlink = KDL::Frame::Identity();
   
        self->dragging = 1;
        if(!(self->selection_hold_on))
        {
        
            // NOTE: cannot use self->object_selection affCollection if selection hold is on;
            object_instance_map_type_::iterator it = self->affCollection->_objects.find(self->object_selection);            
            KDL::Frame T_world_object_future = it->second._gl_object->_T_world_body_future;
        
            //========================================================================================================
            if(it->second._gl_object->is_planar_coupling_active(joint_name))
            {
              token  = "::translate";
              found = joint_name.find(token); 
              if (found!=std::string::npos)
              {
                std::string first_axis_name,second_axis_name;
                it->second._gl_object->get_first_axis_name(joint_name,first_axis_name);
                it->second._gl_object->get_second_axis_name(joint_name,second_axis_name);
                self->joint_marker_pos_on_press = it->second._gl_object->_future_jointpos.find(first_axis_name)->second;
                self->coupled_joint_marker_pos_on_press= it->second._gl_object->_future_jointpos.find(second_axis_name)->second;
              }  
            }
            else 
            //========================================================================================================
            {
              self->joint_marker_pos_on_press = it->second._gl_object->_future_jointpos.find(joint_name)->second;      
            }
            self->marker_offset_on_press << self->ray_hit[0]-T_world_object_future.p[0],self->ray_hit[1]-T_world_object_future.p[1],self->ray_hit[2]-T_world_object_future.p[2];
        }
        else{
           KDL::Frame T_world_object_current = self->otdf_instance_hold._gl_object->_T_world_body;
          if( self->otdf_instance_hold._gl_object->is_jointdof_adjustment_enabled())
          {

            double current_pos, current_vel; 
            //========================================================================================================
            if(self->otdf_instance_hold._gl_object->is_planar_coupling_active(joint_name))
            {
              token  = "::translate";
              found = joint_name.find(token); 
              if (found!=std::string::npos)
              {
                std::string first_axis_name,second_axis_name;
                self->otdf_instance_hold._gl_object->get_first_axis_name(joint_name,first_axis_name);
                self->otdf_instance_hold._gl_object->get_second_axis_name(joint_name,second_axis_name);
                double vel;
                double first_axis_pos, second_axis_pos;
                self->otdf_instance_hold._otdf_instance->getJointState(first_axis_name, first_axis_pos,vel);     
                self->otdf_instance_hold._otdf_instance->getJointState(second_axis_name, second_axis_pos,vel);  
                self->joint_marker_pos_on_press = first_axis_pos;
                self->coupled_joint_marker_pos_on_press=second_axis_pos;
              }  
            }
            else 
            //========================================================================================================
            {
              self->otdf_instance_hold._otdf_instance->getJointState(joint_name, current_pos,current_vel);
              self->joint_marker_pos_on_press = current_pos;  
            }      
            
          }
          self->marker_offset_on_press << self->ray_hit[0]-T_world_object_current.p[0],self->ray_hit[1]-T_world_object_current.p[1],self->ray_hit[2]-T_world_object_current.p[2];
        }

        return 1;// consumed
     }

    bot_viewer_request_redraw(self->viewer);
    return 0; // not consumed if pop up does not come up.
  
}


// ----------------------------------------------------------------------------
static int mouse_release(BotViewer *viewer, BotEventHandler *ehandler, const double ray_start[3], const double ray_dir[3],  const GdkEventButton *event)
{
    RendererAffordances *self = (RendererAffordances*) ehandler->user;
    self->clicked = 0;

    if((ehandler->picking==0)||(self->selection_enabled==0)){
        return 0;
    }  
    if(self->show_popup_onrelease){
        spawn_object_geometry_dblclk_popup(self); // DblClk POPUP!
        self->show_popup_onrelease = 0;
    } 
    if (self->dragging) {
        self->dragging = 0;
        if(self->selection_hold_on && !self->show_popup_onrelease && !self->dblclk_popup){
            if((self->otdf_instance_hold._gl_object->is_bodypose_adjustment_enabled())||(self->otdf_instance_hold._gl_object->is_jointdof_adjustment_enabled()))
               self->affCollection->publish_otdf_instance_to_affstore("AFFORDANCE_TRACK",(self->otdf_instance_hold.otdf_type),self->otdf_instance_hold.uid,self->otdf_instance_hold._otdf_instance); 
        }
    }
    if (ehandler->picking==1)
        ehandler->picking=0; //if picking release picking (Important)

    
    bot_viewer_request_redraw(self->viewer);
    return 0;
}

// ----------------------------------------------------------------------------
static int mouse_motion (BotViewer *viewer, BotEventHandler *ehandler,  const double ray_start[3], const double ray_dir[3],   const GdkEventMotion *event)
{
    RendererAffordances *self = (RendererAffordances*) ehandler->user;
  
    if((!self->dragging)||(ehandler->picking==0)||(self->selection_enabled==0)){
        return 0;
    }
    int64_t now = bot_timestamp_now();
    double dt = (now - self->viewer->last_draw_utime) / 1000000.0;
    if((dt>0.05))
    {
      return 1;
    }
    
    if((self->show_popup_onrelease)||(self->marker_selection  != " ")){
        double t = self->ray_hit_t;
        self->ray_hit_drag << ray_start[0]+t*ray_dir[0], ray_start[1]+t*ray_dir[1], ray_start[2]+t*ray_dir[2];


        Eigen::Vector3f start,dir;
        dir<< ray_dir[0], ray_dir[1], ray_dir[2];
        start<< ray_start[0], ray_start[1], ray_start[2];
    
        //std::cout << "motion\n" << std::endl;
        if(!(self->selection_hold_on)) 
        {
            set_object_desired_state_on_marker_motion(self,start,dir);
        }
        else 
        {
            set_object_current_state_on_marker_motion(self,start,dir);
        }
      
    }
    bot_viewer_request_redraw(self->viewer);
    return 1;
}


// =================================================================================
// 

static void popup_clear_from_affstore(BotGtkParamWidget *pw, void *user)
{
    RendererAffordances *self = (RendererAffordances*) user;

    // create popup warning
    GtkWidget *dialog, *label, *content_area;
    dialog = gtk_dialog_new_with_buttons ("Message",
                                          GTK_WINDOW(self->viewer->window),
                                          GTK_DIALOG_MODAL,
                                          GTK_STOCK_YES,
                                          GTK_RESPONSE_YES,
                                          GTK_STOCK_NO,
                                          GTK_RESPONSE_NO,
                                          NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    label = gtk_label_new ("Clear all affordance from affordance store?");
    //g_signal_connect_swapped (dialog,"response",G_CALLBACK (gtk_widget_destroy),dialog);
    gtk_container_add (GTK_CONTAINER (content_area), label);
    gtk_widget_show_all (dialog);  

    // popup warning
    gint result = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    // handle response
    if (result == GTK_RESPONSE_YES){
        cout << "Sending message to clear all\n";
        map<string, OtdfInstanceStruc>::iterator it;
        for(it = self->affCollection->_objects.begin(); it!=self->affCollection->_objects.end(); it++) {
            self->affCollection->delete_otdf_from_affstore("AFFORDANCE_FIT", it->second.otdf_type, it->second.uid);
        }
    }
}

// =================================================================================
// WIDGET MANAGEMENT AND RENDERER CONSTRUCTION

static void on_param_widget_changed(BotGtkParamWidget *pw, const char *name, void *user)
{
    RendererAffordances *self = (RendererAffordances*) user;
    if(!strcmp(name, PARAM_MANAGE_INSTANCES)) {
        fprintf(stderr,"\nClicked Manage Instances\n");
        spawn_instance_management_popup(self);

    }
    else if (! strcmp (name, PARAM_OTDF_SELECT)) {
        self->otdf_id = bot_gtk_param_widget_get_enum (self->pw, PARAM_OTDF_SELECT);
    }
    else if(!strcmp(name, PARAM_INSTANTIATE)) {
        cout << "\nInstantiating Selected Otdf:  " << self->otdf_filenames[self->otdf_id] << endl;
        self->debugMode = bot_gtk_param_widget_get_bool(pw, PARAM_DEBUG_MODE);
        self->affCollection->create(self->otdf_filenames[self->otdf_id],self->debugMode);
    }
    else if(!strcmp(name, PARAM_CLEAR)) {
        fprintf(stderr,"\nClearing Instantiated Objects\n");
    
        // popup for clear from aff store
        if(!self->debugMode) popup_clear_from_affstore(pw,user);

        self->affCollection->_objects.clear();
        self->stickyHandCollection->_hands.clear();
        self->stickyFootCollection->_feet.clear();
        for( map<string,int >::iterator it = self->instance_cnt.begin(); it!=self->instance_cnt.end(); it++)
            { 
                it->second = 0;
            }
        self->link_selection = " ";
        self->object_selection = " ";
        self->seedSelectionManager->clear();
        self->stickyhand_selection = " ";
        self->stickyfoot_selection = " ";
        self->marker_selection = " ";
        self->selection_hold_on = false;
        bot_viewer_request_redraw(self->viewer);
    }
    else if (! strcmp(name, PARAM_SELECTION)) {
        if (bot_gtk_param_widget_get_bool(pw, PARAM_SELECTION)) {
            //bot_viewer_request_pick (self->viewer, &(self->ehandler));
            self->selection_enabled = 1;
        }
        else{
            self->selection_enabled = 0;
        }
    }
    else if (! strcmp (name, PARAM_LHAND_URDF_SELECT)) {
        self->lhand_urdf_id = bot_gtk_param_widget_get_enum (self->pw, PARAM_LHAND_URDF_SELECT);
    }
    else if (! strcmp (name, PARAM_RHAND_URDF_SELECT)) {
        self->rhand_urdf_id = bot_gtk_param_widget_get_enum (self->pw, PARAM_RHAND_URDF_SELECT);
    }else if(! strcmp(name, PARAM_COLOR_ALPHA)) {
        self->alpha = (float) bot_gtk_param_widget_get_double(pw, PARAM_COLOR_ALPHA);
        bot_viewer_request_redraw(self->viewer);
    }else if(!strcmp(name, PARAM_SHOW_MESH)) {
        self->showMesh = bot_gtk_param_widget_get_bool(pw, PARAM_SHOW_MESH);
        bot_viewer_request_redraw(self->viewer);  
    }else if(!strcmp(name, PARAM_SHOW_BOUNDING_BOX)) {
        self->showBoundingBox = bot_gtk_param_widget_get_bool(pw, PARAM_SHOW_BOUNDING_BOX);
        bot_viewer_request_redraw(self->viewer);  
    }else if(!strcmp(name, PARAM_SHOW_TRIAD)) {
        self->showTriad = bot_gtk_param_widget_get_bool(pw, PARAM_SHOW_TRIAD);
        bot_viewer_request_redraw(self->viewer);  
    }
    else if(!strcmp(name, PARAM_REACHABILITY_FILTER)) {
        self->enableReachabilityFilter = bot_gtk_param_widget_get_bool(pw, PARAM_REACHABILITY_FILTER);
        bot_viewer_request_redraw(self->viewer);  
    }
    else if(!strcmp(name, PARAM_DEBUG_MODE)) {
        self->debugMode = bot_gtk_param_widget_get_bool(pw, PARAM_DEBUG_MODE);
        bot_viewer_request_redraw(self->viewer);  
    }
   else if(! strcmp(name, PARAM_GRASP_OPT_MODE)){
    drc::grasp_opt_mode_t msg;
    msg.utime = self->last_state_msg_timestamp;
    msg.mode = bot_gtk_param_widget_get_enum(self->pw,PARAM_GRASP_OPT_MODE);
    self->lcm->publish("GRASP_OPT_MODE_CONTROL", &msg);
  }
   


}

/*static void keyboardSignalCallback(int keyval, bool is_pressed)
{
  if(is_pressed) 
  {
    cout << "RendererAffordances::KeyPress Signal Received:  Keyval: " << keyval << endl;
  }
  else {
    cout << "RendererAffordances::KeyRelease Signal Received: Keyval: " << keyval << endl;
  }
  // last key event was shift release
  // and selection cnt is greater than one
  if(self->seedSelectionManager->is_multiselect_completed()){
	  self->seedSelectionManager->print();
     //spawn_ee_constraint_sequencer_pop_up();
  }
  
}*/

static void
_free (BotRenderer *renderer)
{
    delete ((RendererAffordances*)(renderer->user));
    // free((RendererAffordances*)(renderer->user));
    //free (renderer);
}

BotRenderer *renderer_affordances_new (BotViewer *viewer, int render_priority, lcm_t *lcm, BotFrames *frames, KeyboardSignalRef signalRef,AffTriggerSignalsRef affTriggerSignalsRef)
{

    //RendererAffordances *self = (RendererAffordances*) calloc (1, sizeof (RendererAffordances)); // Calloc is bad as it prevents having string as members as strings can change size
    RendererAffordances *self = new RendererAffordances;
    self->lcm = boost::shared_ptr<lcm::LCM>(new lcm::LCM(lcm));

    self->frames = frames;

    self->otdf_dir_name = string(getModelsPath()) + "/otdf/"; // getModelsPath gives /drc/software/build/models/

    cout << "searching for otdf files in: "<< self->otdf_dir_name << endl;
    vector<string> otdf_files = vector<string>();
    get_OTDF_filenames_from_dir(self->otdf_dir_name.c_str(),otdf_files);
    cout << "found " << otdf_files.size() << " files"<< endl;
    self->num_otdfs = otdf_files.size();
    self->otdf_names =(char **) calloc(self->num_otdfs, sizeof(char *));
    self->otdf_nums = (int *)calloc(self->num_otdfs, sizeof(int));

    for(size_t i=0;i<otdf_files.size();i++){
        cout << otdf_files[i] << endl;
        self->otdf_filenames.push_back(otdf_files[i]);
        self->instance_cnt.insert(make_pair(otdf_files[i], (int)0));
        self->otdf_names[i]=(char *) otdf_files[i].c_str();
        self->otdf_nums[i] =i;
    }

    self->urdf_dir_name = string(getModelsPath()) + "/mit_gazebo_models/mit_robot_hands/"; // getModelsPath gives /drc/software/build/models/

    cout << "searching for hand urdf files in: "<<  self->urdf_dir_name << endl;
    vector<string> urdf_files = vector<string>();
    get_URDF_filenames_from_dir(self->urdf_dir_name.c_str(),urdf_files);
    cout << "found " << urdf_files.size() << " files"<< endl;
    self->num_urdfs = urdf_files.size();
    self->urdf_names =(char **) calloc(self->num_urdfs, sizeof(char *));
    self->urdf_nums = (int *)calloc(self->num_urdfs, sizeof(int));   

    for(size_t i=0;i<urdf_files.size();i++){
        cout << urdf_files[i] << endl;
        self->urdf_filenames.push_back(urdf_files[i]);
        self->urdf_names[i]=(char *) urdf_files[i].c_str();
        self->urdf_nums[i] =i;
    }
    
    self->affCollection = boost::shared_ptr<AffordanceCollectionManager>(new AffordanceCollectionManager(self->lcm));
    self->stickyHandCollection = boost::shared_ptr<StickyhandCollectionManager>(new StickyhandCollectionManager(self->lcm));
    self->stickyFootCollection = boost::shared_ptr<StickyfootCollectionManager>(new StickyfootCollectionManager(self->lcm));
    
    self->affordanceMsgHandler = boost::shared_ptr<AffordanceCollectionListener>(new AffordanceCollectionListener(self));
    self->robotStateListener = boost::shared_ptr<RobotStateListener>(new RobotStateListener(self));
    self->candidateGraspSeedListener = boost::shared_ptr<CandidateGraspSeedListener>(new CandidateGraspSeedListener(self));
    self->initGraspOptPublisher  = boost::shared_ptr<InitGraspOptPublisher>(new InitGraspOptPublisher(self));
    self->graspOptStatusListener= boost::shared_ptr<GraspOptStatusListener>(new GraspOptStatusListener(self));

    self->reachabilityVerifier=boost::shared_ptr<ReachabilityVerifier>(new ReachabilityVerifier(self));
    //self->keyboardSignalHndlr = boost::shared_ptr<KeyboardSignalHandler>(new KeyboardSignalHandler(signalRef,RendererAffordances::keyboardSignalCallback));
    self->affTriggerSignalsRef = affTriggerSignalsRef;        
    self->keyboardSignalHndlr = boost::shared_ptr<KeyboardSignalHandler>(new KeyboardSignalHandler(signalRef,boost::bind(&RendererAffordances::keyboardSignalCallback,self,_1,_2)));
    self->seedSelectionManager = boost::shared_ptr<SelectionManager>(new SelectionManager(signalRef));
    
    self->T_graspgeometry_lhandinitpos_sandia= KDL::Frame::Identity(); 
    self->T_graspgeometry_rhandinitpos_sandia= KDL::Frame::Identity(); 
    self->T_graspgeometry_lhandinitpos_irobot= KDL::Frame::Identity(); 
    self->T_graspgeometry_rhandinitpos_irobot= KDL::Frame::Identity(); 
    
    self->viewer = viewer;
    memset(&self->renderer,0,sizeof(BotRenderer)); // this is required otherwise we can have uninitialised variables
    self->renderer.draw = _draw;
    self->renderer.destroy = _free;
    self->renderer.name = (char*) RENDERER_NAME;
    self->renderer.user = self;
    self->renderer.enabled = 1;

    BotEventHandler *ehandler = &self->ehandler;
    memset(ehandler,0,sizeof(BotEventHandler)); // this is required otherwise we can have uninitialised variables ehandler->picking
    ehandler->name = (char*) RENDERER_NAME;
    ehandler->enabled = 1;
    ehandler->pick_query = pick_query;
    ehandler->key_press = NULL;
    ehandler->hover_query = NULL;
    ehandler->mouse_press = mouse_press;
    ehandler->mouse_release = mouse_release;
    ehandler->mouse_motion = mouse_motion;
    ehandler->user = self;

  

    bot_viewer_add_event_handler(viewer, &self->ehandler, render_priority);

    self->pw = BOT_GTK_PARAM_WIDGET(bot_gtk_param_widget_new());

    self->otdf_id= 0; // default file
    
    
     std::vector<std::string>::const_iterator found;
     found = std::find (self->urdf_filenames.begin(), self->urdf_filenames.end(), "sandia_hand_left");
     if (found != self->urdf_filenames.end()) 
         self->lhand_urdf_id= found - self->urdf_filenames.begin();
     else    
         self->lhand_urdf_id= 1; // default file
     found = std::find (self->urdf_filenames.begin(), self->urdf_filenames.end(), "sandia_hand_right");
     if (found != self->urdf_filenames.end()) 
         self->rhand_urdf_id= found - self->urdf_filenames.begin();
     else    
         self->rhand_urdf_id= 3; // default file     
    self->alpha = 1.0; // default opacity is full on
    self->showMesh = false;
    self->enableReachabilityFilter = false;
    self->debugMode = false;
    self->selection_enabled = true;      
    self->clicked = false;	
    self->dragging = false;
    self->show_popup_onrelease = false;
    self->selection_hold_on = false;
  
    bot_gtk_param_widget_add_separator (self->pw,"Objects");
    bot_gtk_param_widget_add_enumv (self->pw, PARAM_OTDF_SELECT, BOT_GTK_PARAM_WIDGET_MENU, 
                                    self->otdf_id,
                                    self->num_otdfs,
                                    (const char **)  self->otdf_names,
                                    self->otdf_nums);

    bot_gtk_param_widget_add_buttons(self->pw,PARAM_INSTANTIATE, NULL);
    bot_gtk_param_widget_add_buttons(self->pw, PARAM_MANAGE_INSTANCES, NULL);
    bot_gtk_param_widget_add_buttons(self->pw,PARAM_CLEAR, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SELECTION, 0, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_OPT_POOL_READY, 0, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SHOW_MESH, 0, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SHOW_BOUNDING_BOX, 0, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SHOW_TRIAD, 0, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX,
                                      PARAM_REACHABILITY_FILTER, 0, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX,
                                      PARAM_DEBUG_MODE, 1, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SHOW_PROPOSED_MANIP_MAP, 0, NULL);  

    bot_gtk_param_widget_add_double (self->pw, PARAM_COLOR_ALPHA,
                                     BOT_GTK_PARAM_WIDGET_SLIDER,
                                     0, 1, 0.001, 1);   
  
    bot_gtk_param_widget_add_separator (self->pw,"Sticky Hands");
    bot_gtk_param_widget_add_enumv (self->pw, PARAM_LHAND_URDF_SELECT, BOT_GTK_PARAM_WIDGET_MENU, 
                                    self->lhand_urdf_id,
                                    self->num_urdfs,
                                    (const char **)  self->urdf_names,
                                    self->urdf_nums);
    bot_gtk_param_widget_add_enumv (self->pw, PARAM_RHAND_URDF_SELECT, BOT_GTK_PARAM_WIDGET_MENU, 
                                    self->rhand_urdf_id,
                                    self->num_urdfs,
                                    (const char **)  self->urdf_names,
                                    self->urdf_nums); 
   bot_gtk_param_widget_add_enum(self->pw, PARAM_GRASP_OPT_MODE, BOT_GTK_PARAM_WIDGET_MENU,drc::grasp_opt_mode_t::OPT_ON, 
                                   "OptOn", drc::grasp_opt_mode_t::OPT_ON,
                                   "OptOff", drc::grasp_opt_mode_t::OPT_OFF, NULL);                                 
  
    g_signal_connect(G_OBJECT(self->pw), "changed", G_CALLBACK(on_param_widget_changed), self);
    self->renderer.widget = GTK_WIDGET(self->pw);

	bot_gtk_param_widget_set_bool(self->pw, PARAM_SELECTION,self->selection_enabled);
	bool optpoolready = self->graspOptStatusListener->isOptPoolReady();
	bot_gtk_param_widget_set_bool(self->pw,PARAM_OPT_POOL_READY,optpoolready);
	bot_gtk_param_widget_set_bool(self->pw,PARAM_REACHABILITY_FILTER,self->enableReachabilityFilter);     
	bot_gtk_param_widget_set_bool(self->pw,PARAM_DEBUG_MODE,self->debugMode);
  
   return &self->renderer;
}

void setup_renderer_affordances(BotViewer *viewer, int render_priority, lcm_t *lcm, BotFrames *frames, KeyboardSignalRef signalRef,AffTriggerSignalsRef affTriggerSignalsRef)
{
    bot_viewer_add_renderer_on_side(viewer, renderer_affordances_new(viewer, render_priority, lcm, frames,signalRef,affTriggerSignalsRef), render_priority, 0); // 0= add on left hand side
}

void setup_renderer_affordances(BotViewer *viewer, int render_priority, lcm_t *lcm, KeyboardSignalRef signalRef,AffTriggerSignalsRef affTriggerSignalsRef)
{
    bot_viewer_add_renderer_on_side(viewer, renderer_affordances_new(viewer, render_priority, lcm, NULL,signalRef,affTriggerSignalsRef), render_priority, 0); // 0= add on left hand side
}

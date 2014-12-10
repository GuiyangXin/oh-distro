
#include "renderer_robot_plan.hpp"
#include "RobotPlanListener.hpp"
#include "lcmtypes/drc/ee_goal_t.hpp"
#include "lcmtypes/drc/ee_manip_gain_t.hpp"
#include "lcmtypes/drc/manip_plan_control_t.hpp"
#include "lcmtypes/drc/plan_execution_speed_t.hpp"
#include "lcmtypes/drc/plan_adjust_mode_t.hpp"
#include "lcmtypes/drc_atlas_status_t.h"
#include "lcmtypes/drc_robot_plan_t.h"
#include "lcmtypes/drc_utime_t.h"

#define PARAM_KP_DEFAULT 5
#define PARAM_KD_DEFAULT 1
#define PARAM_KP_MIN 0
#define PARAM_KD_MIN 0
#define PARAM_KP_MAX 100
#define PARAM_KD_MAX 100
#define PARAM_KP_INC 1
#define PARAM_KD_INC 1

#define RENDERER_NAME "Planning"
#define PARAM_SELECTION "Enable Selection"
#define PARAM_WIRE "Show BBoxs For Meshes"  
#define PARAM_HIDE "Hide Plan"  
//#define PARAM_USE_COLORMAP "Use Colormap"
#define PARAM_PLAN_PART "Part of Plan"  
#define PARAM_SHOW_DURING_CONTROL "During Control"  
#define DRAW_PERSIST_SEC 4
#define PARAM_START_PLAN "Start Planning"
#define PARAM_SEND_COMMITTED_PLAN "Send Plan"
#define PARAM_ADJUST_ENDSTATE "Adjust end keyframe"
#define PARAM_SHOW_FULLPLAN "Show Full Plan"	
#define PARAM_SHOW_KEYFRAMES "Show Keyframes"
#define PARAM_SSE_KP_LEFT "Kp_L"  
#define PARAM_SSE_KD_LEFT "Kd_L"  
#define PARAM_SSE_KP_RIGHT "Kp_R"  
#define PARAM_SSE_KD_RIGHT "Kd_R"
#define PARAM_MANIP_PLAN_MODE "ManipPlnr Mode"
#define PARAM_EXEC_SPEED "EE Speed Limit(cm/s)"
#define PARAM_EXEC_ANG_SPEED "Joint Speed Limit(deg/s)"
#define PARAM_UPDATE_PLANNER_PARAMS "Update Params"
#define PARAM_PLAN_ADJUST_MODE "Plan Adjustment Filter"
#define PARAM_MANIP_PLAN_INITSEED_MODE "ManipPlanFromCurrentState"
#define PARAM_PLAN_USING_BDI_HEIGHT_MODE "Plan & Control w BDI Height"
#define PARAM_ADJUST_PLAN_TO_CURRENT_POSE "Adjust Plan To Current Pose"
#define PARAM_ADJUST_PLAN_AND_REACH "Achieve First Posture"
#define PARAM_COMPENSATE_LAST_FRAME_FOR_SSE "Compensate for SSE"

using namespace std;
using namespace boost;
using namespace renderer_robot_plan;

static void
_renderer_free (BotRenderer *super)
{
  RendererRobotPlan *self = (RendererRobotPlan*) super->user;
  free(self);
}


//=================================


// Convert number to jet colour coordinates
// In: number between 0-->1
// Out: rgb jet colours 0->1
// http://metastine.com/2011/01/implementing-a-continuous-jet-colormap-function-in-glsl/
static inline void jet_rgb(float value,float rgb[]){
  float fourValue = (float) 4 * value;
  rgb[0]   = std::min(fourValue - 1.5, -fourValue + 4.5);
  rgb[1] = std::min(fourValue - 0.5, -fourValue + 3.5);
  rgb[2]  = std::min(fourValue + 0.5, -fourValue + 2.5);
  for (int i=0;i<3;i++){
   if (rgb[i] <0) {
     rgb[i] =0;
   }else if (rgb[i] >1){
     rgb[i] =1;
   }
  }
}


static void 
draw_state(BotViewer *viewer, BotRenderer *super, uint i, float rgb[]){

  float c[3] = {0.3,0.3,0.6}; // light blue
  float alpha = 0.4;
  RendererRobotPlan *self = (RendererRobotPlan*) super->user;
 
  if((self->use_colormap)&&(self->displayed_plan_index==-1)&&(!self->robotPlanListener->_is_manip_plan)) {
    // Each model Jet: blue to red
    float j = (float)i/ (self->robotPlanListener->_gl_robot_list.size() -1);
    jet_rgb(j,c);
  }else{
    c[0] = rgb[0]; c[1] = rgb[1]; c[2] = rgb[2];
  }
  
  glColor4f(c[0],c[1],c[2], alpha);
  
  
  self->robotPlanListener->_gl_robot_list[i]->show_bbox(self->visualize_bbox);
  self->robotPlanListener->_gl_robot_list[i]->enable_link_selection(self->selection_enabled);
  //if((*self->selection)!=" ")
  self->robotPlanListener->_gl_robot_list[i]->highlight_link((*self->selection));
  self->robotPlanListener->_gl_robot_list[i]->draw_body (c,alpha);  
}



static void 
_renderer_draw (BotViewer *viewer, BotRenderer *super)
{
  RendererRobotPlan *self = (RendererRobotPlan*) super->user;
  // if hide is enabled - then dont draw the plan:
  if (bot_gtk_param_widget_get_bool(self->pw, PARAM_HIDE)) {
     return;
  }
  
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  //-draw 
  //glPointSize(5.0f);
  //glBegin(GL_POINTS);
  glEnable(GL_LIGHTING);
  glEnable(GL_COLOR_MATERIAL);
  glEnable(GL_BLEND);
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

  if((self->selection_enabled)&&(self->clicked)){
    glLineWidth (3.0);
    glPushMatrix();
    glBegin(GL_LINES);
    glVertex3f(self->ray_start[0], self->ray_start[1],self->ray_start[2]); // object coord
    glVertex3f(self->ray_end[0], self->ray_end[1],self->ray_end[2]);
    glEnd();
    glPopMatrix();
  }

  int plan_size =   self->robotPlanListener->_gl_robot_list.size();
  if (plan_size ==0){ // nothing to renderer
  // on receipt of a apprved footstep plan, the current plan is purged in waiting for a new walking plan.
  // if any plan execution/approval dock's exist, they will be destroyed.
    if(self->plan_execution_dock!=NULL){
      gtk_widget_destroy(self->plan_execution_dock);
      self->plan_execution_dock= NULL;  
    } 
    if(self->multiapprove_plan_execution_dock!=NULL)
    {
      gtk_widget_destroy(self->multiapprove_plan_execution_dock);
      self->multiapprove_plan_execution_dock= NULL;  
    } 
    if(self->plan_approval_dock!=NULL){
      gtk_widget_destroy(self->plan_approval_dock);
      self->plan_approval_dock= NULL;  
    }   
    return;
  }

  self->selected_keyframe_index=-1; // clear selected keyframe index
  
  bool markeractive = false;  
    
  if (self->show_fullplan){
    int max_num_states = 20;
    int inc =1;
    int totol_states = self->robotPlanListener->_gl_robot_list.size();
    if ( totol_states > max_num_states) {
      inc = ceil( totol_states/max_num_states);
      inc = min(max(inc,1),max_num_states);	
    }    
    //std::cout << "totol_states is " << totol_states << "\n";    
    //std::cout << "inc is " << inc << "\n";    

    float c[3] = {0.3,0.3,0.6}; // light blue (holder)
    for(uint i = 0; i < totol_states; i=i+inc){//_gl_robot_list.size(); i++){ 
      if(!markeractive)
        draw_state(viewer,super,i,c);
    }
    self->displayed_plan_index = -1;    
  }else{
    double plan_part = bot_gtk_param_widget_get_double(self->pw, PARAM_PLAN_PART);
    uint w_plan = (uint) round(plan_part* (plan_size -1));
    //printf("                                  Show around %f of %d    %d\n", plan_part, plan_size, w_plan);
    self->displayed_plan_index = w_plan;
    
    float c[3] = {0.3,0.3,0.6}; // light blue
    if(!markeractive)
      draw_state(viewer,super,w_plan,c);
  }
  
}

  
//========================= Event Handling ================

static double pick_query (BotViewer *viewer, BotEventHandler *ehandler, const double ray_start[3], const double ray_dir[3])
{
  RendererRobotPlan *self = (RendererRobotPlan*) ehandler->user;
  if((self->selection_enabled==0)||(bot_gtk_param_widget_get_bool(self->pw, PARAM_HIDE))){
    return -1.0;
  }
  //fprintf(stderr, "RobotStateRenderer Pick Query Active\n");
  Eigen::Vector3f from,to;
  from << ray_start[0], ray_start[1], ray_start[2];

  Eigen::Vector3f plane_normal,plane_pt;
  plane_normal << 0,0,1;
  if(ray_start[2]<0)
      plane_pt << 0,0,10;
  else
      plane_pt << 0,0,-10;
  double lambda1 = ray_dir[0] * plane_normal[0]+
                   ray_dir[1] * plane_normal[1] +
                   ray_dir[2] * plane_normal[2];
   // check for degenerate case where ray is (more or less) parallel to plane
    if (fabs (lambda1) < 1e-9) return -1.0;

   double lambda2 = (plane_pt[0] - ray_start[0]) * plane_normal[0] +
       (plane_pt[1] - ray_start[1]) * plane_normal[1] +
       (plane_pt[2] - ray_start[2]) * plane_normal[2];
   double t = lambda2 / lambda1;// =1;
  
  to << ray_start[0]+t*ray_dir[0], ray_start[1]+t*ray_dir[1], ray_start[2]+t*ray_dir[2];
 
  self->ray_start = from;
  self->ray_end = to;
  self->ray_hit_t = t;
  self->ray_hit_drag = to;
  self->ray_hit = to; 
  
  //

  Eigen::Vector3f hit_pt;
  collision::Collision_Object * intersected_object = NULL;
  double shortest_distance = -1;
  self->selected_plan_index= 0;

  if(self->robotPlanListener->_is_manip_plan)  
  {
    shortest_distance = get_shortest_distance_between_keyframes_and_markers(self,from,to);
  }
  else   
  {
    shortest_distance = get_shortest_distance_from_a_plan_frame(self,from,to);
  }

  //std::cout  << "RobotStateRenderer distance " << -1.0 << std::endl;
  return shortest_distance;
}

static int mouse_press (BotViewer *viewer, BotEventHandler *ehandler, const double ray_start[3], const double ray_dir[3], const GdkEventButton *event)
{
  RendererRobotPlan *self = (RendererRobotPlan*) ehandler->user;
  if((ehandler->picking==0)||(self->selection_enabled==0)){
    //fprintf(stderr, "Ehandler Not active\n");
   (*self->selection)  = " ";
    return 0;
  }
 // fprintf(stderr, "RobotPlanRenderer Ehandler Activated\n");
  self->clicked = 1;
  //fprintf(stderr, "Mouse Press : %f,%f\n",ray_start[0], ray_start[1]);
  collision::Collision_Object * intersected_object = NULL;
   if((self->robotPlanListener->_is_manip_plan) && 
      (self->selected_keyframe_index!=-1) &&
      (self->robotPlanListener->_gl_robot_keyframe_list.size()>0) &&
      (self->selected_keyframe_index > 0) &&
      (self->selected_keyframe_index < self->robotPlanListener->_gl_robot_keyframe_list.size())
     )
   {
   // cout << "keyframe: " << self->selected_keyframe_index << " " << self->robotPlanListener->_gl_robot_keyframe_list.size()<< endl;
    self->robotPlanListener->_gl_robot_keyframe_list[self->selected_keyframe_index]->_collision_detector->ray_test( self->ray_start, self->ray_end, intersected_object ); 
    if( intersected_object != NULL ){
        std::cout << "prev selection :" << (*self->selection)  <<  std::endl;
        std::cout << "intersected :" << intersected_object->id().c_str() <<  std::endl;
        (*self->selection)  = std::string(intersected_object->id().c_str());
        
	      std::string body_name = self->robotPlanListener->_gl_robot_keyframe_list[self->selected_keyframe_index]->_unique_name; 
        self->robotPlanListener->_gl_robot_keyframe_list[self->selected_keyframe_index]->highlight_body(body_name);
        //self->robotPlanListener->_gl_robot_keyframe_list[self->selected_keyframe_index]->highlight_link((*self->selection));
     }

   }
   else{
    if(self->selected_plan_index < self->robotPlanListener->_gl_robot_list.size())
    {
      self->robotPlanListener->_gl_robot_list[self->selected_plan_index]->_collision_detector->ray_test( self->ray_start, self->ray_end, intersected_object );
      if( intersected_object != NULL ){
          std::cout << "prev selection :" << (*self->selection)  <<  std::endl;
          std::cout << "intersected :" << intersected_object->id().c_str() <<  std::endl;
          (*self->selection)  = std::string(intersected_object->id().c_str());
          self->robotPlanListener->_gl_robot_list[self->selected_plan_index]->highlight_link((*self->selection));
       }// end if
     }// end if
   }


    if((event->button==3) &&(event->type==GDK_2BUTTON_PRESS)) // right dbl clk
    {
      string name(self->renderer.name);
      self->_renderer_foviate=!self->_renderer_foviate;
      (*self->_rendererFoviationSignalRef)((void*)self->viewer,name,self->_renderer_foviate); 
    }

  if((self->robotPlanListener->_is_manip_plan) && 
     (((*self->selection)  != " ")||((*self->marker_selection)  != " ")) &&
     (event->button==1) &&
     (self->selected_keyframe_index< self->robotPlanListener->_gl_robot_keyframe_list.size()) &&
     (event->type==GDK_2BUTTON_PRESS) 
    )
  {

   if((*self->marker_selection)  == " ")
    cout << "DblClk: " << (*self->selection) << endl;
   else
    cout << "DblClk on Marker: " << (*self->marker_selection) << endl;
    // On double click create/toggle  local copies of right and left sticky hand duplicates and spawn them with markers
    if(self->selected_keyframe_index!=-1)// dbl clk on keyframe then toogle 
    { 

      bool markeractive = false;

      bool toggle=true;
      if (self->robotPlanListener->is_in_motion(self->selected_keyframe_index)){
         toggle = !markeractive;
      }

      
      if(!toggle){
        (*self->marker_selection)  = " ";
      }
    }
   return 1;// consumed if pop up comes up.    
  }
  else if(((*self->marker_selection)  != " "))
  {
    self->dragging = 1;
    
    KDL::Frame T_world_marker;
    //Marker Foviation Logic
    self->marker_offset_on_press << self->ray_hit[0]-T_world_marker.p[0],self->ray_hit[1]-T_world_marker.p[1],self->ray_hit[2]-T_world_marker.p[2]; 
    std::cout << "RendererRobotPlan: Event is consumed" <<  std::endl;
    return 1;// consumed
  }

  bot_viewer_request_redraw(self->viewer);
  return 0;
}


static int 
mouse_release (BotViewer *viewer, BotEventHandler *ehandler, const double ray_start[3], 
    const double ray_dir[3], const GdkEventButton *event)
{
  RendererRobotPlan *self = (RendererRobotPlan*) ehandler->user;
  self->clicked = 0;
  if((ehandler->picking==0)||(self->selection_enabled==0)){
    //fprintf(stderr, "Ehandler Not active\n");
    return 0;
  }
  if (self->dragging) {
    self->dragging = 0;
    string channel = "MANIP_PLAN_CONSTRAINT";
    
    Eigen::Vector3f diff=self->ray_hit_drag-self->ray_hit;
    double movement = diff.norm();

  }
  if (ehandler->picking==1)
    ehandler->picking=0; //release picking(IMPORTANT)
  bot_viewer_request_redraw(self->viewer);
  return 0;
}


static int mouse_motion (BotViewer *viewer, BotEventHandler *ehandler,  const double ray_start[3], const double ray_dir[3],   const GdkEventMotion *event)
{
  RendererRobotPlan *self = (RendererRobotPlan*) ehandler->user;
  
  if((!self->dragging)||(ehandler->picking==0)){
    return 0;
  }
  
  if((*self->marker_selection)  != " "){
    double t = self->ray_hit_t;
    self->ray_hit_drag << ray_start[0]+t*ray_dir[0], ray_start[1]+t*ray_dir[1], ray_start[2]+t*ray_dir[2];
    //TODO: Add support for joint dof markers for keyframes [switch via check box in main pane instead of a popup]
    //TODO: JointDof constraint comms with planner (pain) 
      Eigen::Vector3f start,dir;
    dir<< ray_dir[0], ray_dir[1], ray_dir[2];
    start<< ray_start[0], ray_start[1], ray_start[2]; 
    adjust_keyframe_on_marker_motion(self,start,dir);
    // cout << (*self->marker_selection) << ": mouse drag\n";
  }
  bot_viewer_request_redraw(self->viewer);
  return 1;
}



/*static void keyboardSignalCallback(int keyval, bool is_pressed)
{
  if(is_pressed) 
  {
    cout << "RendererRobotPlan::KeyPress Signal Received:  Keyval: " << keyval << endl;
  }
  else {
    cout << "RendererRobotPlan::KeyRelease Signal Received: Keyval: " << keyval << endl;
  }
}*/

// ------------------------END Event Handling-------------------------------------------

static void onRobotUtime (const lcm_recv_buf_t * buf, const char *channel, 
                               const drc_utime_t *msg, void *user){
  RendererRobotPlan *self = (RendererRobotPlan*) user;
  self->robot_utime = msg->utime;
}

static void onPlanExecuteEvent (const lcm_recv_buf_t * buf, const char *channel, 
                               const drc_utime_t *msg, void *user)
{
    RendererRobotPlan *self = (RendererRobotPlan*) user;
    if((!self->robotPlanListener->_current_plan_committed)&&(self->plan_execute_button!=NULL))
    {
      gtk_widget_destroy (self->plan_execute_button);
      self->plan_execute_button= NULL;
      self->robotPlanListener->_current_plan_committed = true;
    }
}

static void onPlanRejectEvent (const lcm_recv_buf_t * buf, const char *channel, 
                               const drc_robot_plan_t *msg, void *user)
{
    RendererRobotPlan *self = (RendererRobotPlan*) user;
    if((self->plan_execution_dock!=NULL)||(self->multiapprove_plan_execution_dock!=NULL))
    {
      self->robotPlanListener->purge_current_plan();
      self->plan_execute_button = NULL;
      if(self->robotPlanListener->is_multi_approval_plan()){
        gtk_widget_destroy(self->multiapprove_plan_execution_dock);
        self->multiapprove_plan_execution_dock= NULL;    
      }
      else {
        gtk_widget_destroy(self->plan_execution_dock);
        self->plan_execution_dock= NULL;
      }
    }
}

static void
onAtlasStatus(const lcm_recv_buf_t * buf, const char *channel, const drc_atlas_status_t *msg, void *user_data){
  RendererRobotPlan *self = (RendererRobotPlan*) user_data;
  self->atlas_state = msg->behavior;
  self->atlas_status_utime = msg->utime; 
}

static void update_planar_params( void *user)
{
    RendererRobotPlan *self = (RendererRobotPlan*) user;  
                            
     {                                    
       drc::plan_execution_speed_t msg;
       msg.utime = bot_timestamp_now();
       msg.speed = (M_PI/180)*bot_gtk_param_widget_get_double(self->pw, PARAM_EXEC_ANG_SPEED); //pub in rads/s; converting from deg/s
       self->lcm->publish("DESIRED_JOINT_SPEED", &msg);
       msg.speed = (0.01)*bot_gtk_param_widget_get_double(self->pw, PARAM_EXEC_SPEED);//pub in m/s; converting from cm/s
       self->lcm->publish("DESIRED_EE_ARC_SPEED", &msg);
     }
     
     {
       drc::manip_plan_control_t msg;
       msg.utime= bot_timestamp_now();
       msg.mode = bot_gtk_param_widget_get_enum(self->pw,PARAM_MANIP_PLAN_MODE);
        if(msg.mode!=1)
        {
          self->adjust_endstate = true;
          bot_gtk_param_widget_set_bool(self->pw, PARAM_ADJUST_ENDSTATE,self->adjust_endstate);
        }
       self->lcm->publish("MANIP_PLANNER_MODE_CONTROL", &msg);
     }
     {  
       drc::plan_adjust_mode_t msg;
       msg.utime = bot_timestamp_now();
       msg.mode = (int)bot_gtk_param_widget_get_bool(self->pw,PARAM_MANIP_PLAN_INITSEED_MODE);
       self->lcm->publish("MANIP_PLAN_FROM_CURRENT_STATE", &msg);
     }
              
     {
       drc::plan_adjust_mode_t msg;
       msg.utime = bot_timestamp_now();
       msg.mode = (int)bot_gtk_param_widget_get_bool(self->pw,PARAM_PLAN_USING_BDI_HEIGHT_MODE);
       self->lcm->publish("PLAN_USING_BDI_HEIGHT", &msg);
     }

}
static void on_param_widget_changed(BotGtkParamWidget *pw, const char *name, void *user)
{
  RendererRobotPlan *self = (RendererRobotPlan*) user;
  if (! strcmp(name, PARAM_SELECTION)) {
    self->selection_enabled = bot_gtk_param_widget_get_bool(pw, PARAM_SELECTION);
  }  else if(! strcmp(name, PARAM_WIRE)) {
    self->visualize_bbox = bot_gtk_param_widget_get_bool(pw, PARAM_WIRE);
  //}  else if(! strcmp(name,PARAM_USE_COLORMAP)) {
  //  self->use_colormap	= bot_gtk_param_widget_get_bool(pw, PARAM_USE_COLORMAP);
  }  else if(! strcmp(name,PARAM_ADJUST_ENDSTATE)) {
    self->adjust_endstate = bot_gtk_param_widget_get_bool(pw, PARAM_ADJUST_ENDSTATE);
  }  else if(! strcmp(name,PARAM_SHOW_FULLPLAN)) {
    self->show_fullplan = bot_gtk_param_widget_get_bool(pw, PARAM_SHOW_FULLPLAN);
  }  else if(! strcmp(name,PARAM_SHOW_KEYFRAMES)) {
    self->show_keyframes = bot_gtk_param_widget_get_bool(pw, PARAM_SHOW_KEYFRAMES);
  }else if(!strcmp(name,PARAM_SEND_COMMITTED_PLAN)){
    self->lcm->publish("COMMITTED_ROBOT_PLAN", &(self->robotPlanListener->_received_plan) );
  }
  else if(!strcmp(name,  PARAM_EXEC_SPEED)){ 
    drc::plan_execution_speed_t msg;
    msg.utime = bot_timestamp_now();
    msg.speed = (0.01)*bot_gtk_param_widget_get_double(self->pw, PARAM_EXEC_SPEED);//pub in m/s; converting from cm/s
    self->lcm->publish("DESIRED_EE_ARC_SPEED", &msg);
  }
  else if(!strcmp(name,  PARAM_EXEC_ANG_SPEED)){ 
    drc::plan_execution_speed_t msg;
    msg.utime = bot_timestamp_now();
    msg.speed = (M_PI/180)*bot_gtk_param_widget_get_double(self->pw, PARAM_EXEC_ANG_SPEED); //pub in rads/s; converting from deg/s
    self->lcm->publish("DESIRED_JOINT_SPEED", &msg);
  }
  else if(!strcmp(name,PARAM_UPDATE_PLANNER_PARAMS))
  {
     update_planar_params(self);
  }
  else if(! strcmp(name, PARAM_MANIP_PLAN_MODE)){
    drc::manip_plan_control_t msg;
    msg.utime = self->robot_utime;
    msg.mode = bot_gtk_param_widget_get_enum(self->pw,PARAM_MANIP_PLAN_MODE);
    if(msg.mode!=1)
    {
      self->adjust_endstate = true;
      bot_gtk_param_widget_set_bool(pw, PARAM_ADJUST_ENDSTATE,self->adjust_endstate);
    }
    self->lcm->publish("MANIP_PLANNER_MODE_CONTROL", &msg);
  }
  else if(! strcmp(name, PARAM_ADJUST_PLAN_TO_CURRENT_POSE)){
    drc::plan_adjust_mode_t msg;
    msg.utime = self->robot_utime;
    msg.mode = bot_gtk_param_widget_get_enum(self->pw,PARAM_PLAN_ADJUST_MODE);
    self->lcm->publish("ADJUST_PLAN_TO_CURRENT_PELVIS_POSE", &msg);
  }
  else if(! strcmp(name, PARAM_ADJUST_PLAN_AND_REACH)){
    drc::plan_adjust_mode_t msg;
    msg.utime = self->robot_utime;
    msg.mode = bot_gtk_param_widget_get_enum(self->pw,PARAM_PLAN_ADJUST_MODE);
    self->lcm->publish("ADJUST_PLAN_AND_REACH", &msg);
  }
  else if(! strcmp(name, PARAM_MANIP_PLAN_INITSEED_MODE)){
    drc::plan_adjust_mode_t msg;
    msg.utime = self->robot_utime;
    msg.mode = (int)bot_gtk_param_widget_get_bool(self->pw,PARAM_MANIP_PLAN_INITSEED_MODE);
    self->lcm->publish("MANIP_PLAN_FROM_CURRENT_STATE", &msg);
  }
  else if(! strcmp(name, PARAM_PLAN_USING_BDI_HEIGHT_MODE)){
    drc::plan_adjust_mode_t msg;
    msg.utime = self->robot_utime;
    msg.mode = (int)bot_gtk_param_widget_get_bool(self->pw,PARAM_PLAN_USING_BDI_HEIGHT_MODE);
    self->lcm->publish("PLAN_USING_BDI_HEIGHT", &msg);
  }
  else if(! strcmp(name, PARAM_COMPENSATE_LAST_FRAME_FOR_SSE)){
    drc::plan_adjust_mode_t msg;
    msg.utime = self->robot_utime;
    msg.mode = (int) bot_gtk_param_widget_get_bool(self->pw,PARAM_PLAN_ADJUST_MODE);
    self->lcm->publish("MOVE_TO_COMPENSATE_SSE", &msg);
  }  
  bot_viewer_request_redraw(self->viewer);
  
}

void 
setup_renderer_robot_plan(BotViewer *viewer, int render_priority, lcm_t *lcm, int operation_mode, KeyboardSignalRef signalRef,AffTriggerSignalsRef affTriggerSignalsRef,RendererFoviationSignalRef rendererFoviationSignalRef)
{
    RendererRobotPlan *self = (RendererRobotPlan*) calloc (1, sizeof (RendererRobotPlan));
    self->lcm = boost::shared_ptr<lcm::LCM>(new lcm::LCM(lcm));
    
    self->robotPlanListener = boost::shared_ptr<RobotPlanListener>(new RobotPlanListener(self->lcm, viewer, operation_mode));
    //self->keyboardSignalHndlr = boost::shared_ptr<KeyboardSignalHandler>(new KeyboardSignalHandler(signalRef,keyboardSignalCallback));
    self->keyboardSignalHndlr = boost::shared_ptr<KeyboardSignalHandler>(new KeyboardSignalHandler(signalRef,boost::bind(&RendererRobotPlan::keyboardSignalCallback,self,_1,_2)));
    self->affTriggerSignalsHndlr = boost::shared_ptr<AffTriggerSignalsHandler>(new AffTriggerSignalsHandler(affTriggerSignalsRef,boost::bind(&RendererRobotPlan::affTriggerSignalsCallback,self,_1,_2,_3,_4)));
    self->_rendererFoviationSignalRef = rendererFoviationSignalRef;
    
    BotRenderer *renderer = &self->renderer;

    renderer->draw = _renderer_draw;
    renderer->destroy = _renderer_free;

    renderer->widget = bot_gtk_param_widget_new();
    renderer->name = (char *) RENDERER_NAME;
    if (operation_mode ==1){
      renderer->name =(char *) "Robot Plan Loopback";
    }else if(operation_mode ==2){
      renderer->name =(char *) "Robot Plan LB Compressed";      
    }
    renderer->user = self;
    renderer->enabled = 1;

    self->viewer = viewer;
    
    self->pw = BOT_GTK_PARAM_WIDGET(renderer->widget);
    
    // C-style subscribe:
    drc_utime_t_subscribe(self->lcm->getUnderlyingLCM(),"ROBOT_UTIME",onRobotUtime,self); 
    drc_atlas_status_t_subscribe(self->lcm->getUnderlyingLCM(),"ATLAS_STATUS",onAtlasStatus,self);
    drc_utime_t_subscribe(self->lcm->getUnderlyingLCM(),"ROBOT_PLAN_EXECUTE_EVENT",onPlanExecuteEvent,self);
    drc_robot_plan_t_subscribe(self->lcm->getUnderlyingLCM(),"REJECTED_ROBOT_PLAN",onPlanRejectEvent,self);

    
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SELECTION, 0, NULL);
    // disabled_for_cleanup bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_WIRE, 0, NULL);

    // commented out unused buttons:
    // bot_gtk_param_widget_add_buttons(self->pw, PARAM_START_PLAN, NULL);
    // bot_gtk_param_widget_add_buttons(self->pw, PARAM_SEND_COMMITTED_PLAN, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_HIDE, 0, NULL);
    // bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_USE_COLORMAP, 0, NULL);
    self->adjust_endstate = true;
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_ADJUST_ENDSTATE, self->adjust_endstate, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SHOW_FULLPLAN, 1, NULL);
    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SHOW_KEYFRAMES, 1, NULL);
 
    
    bot_gtk_param_widget_add_double (self->pw, PARAM_PLAN_PART,
                                   BOT_GTK_PARAM_WIDGET_SLIDER, 0, 1, 0.005, 1);    
    // disabled_for_cleanup bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_SHOW_DURING_CONTROL, 1, NULL);
                                                    
    bot_gtk_param_widget_add_separator (self->pw,"Replanning");
    bot_gtk_param_widget_add_enum(self->pw, PARAM_PLAN_ADJUST_MODE, BOT_GTK_PARAM_WIDGET_MENU,drc::plan_adjust_mode_t::LEFT_HAND, 
                                       "LHnd", drc::plan_adjust_mode_t::LEFT_HAND,
                                       "RHnd", drc::plan_adjust_mode_t::RIGHT_HAND,
                                       "BothHnds", drc::plan_adjust_mode_t::BOTH_HANDS,
                                       "LFoot", drc::plan_adjust_mode_t::LEFT_FOOT,
                                       "RFoot", drc::plan_adjust_mode_t::RIGHT_FOOT,
                                       "BothFeet", drc::plan_adjust_mode_t::BOTH_FEET,
                                       "All", drc::plan_adjust_mode_t::ALL, NULL);
    
    bot_gtk_param_widget_add_buttons(self->pw, PARAM_ADJUST_PLAN_TO_CURRENT_POSE, NULL);
    bot_gtk_param_widget_add_buttons(self->pw, PARAM_ADJUST_PLAN_AND_REACH, NULL);
    bot_gtk_param_widget_add_buttons(self->pw, PARAM_COMPENSATE_LAST_FRAME_FOR_SSE, NULL);
                                       
    bot_gtk_param_widget_add_separator (self->pw,"Planner Params"); 
    bot_gtk_param_widget_add_double(self->pw, PARAM_EXEC_SPEED, BOT_GTK_PARAM_WIDGET_SPINBOX,
                                                1, 20, 0.5, 10);                                     
    bot_gtk_param_widget_add_double(self->pw, PARAM_EXEC_ANG_SPEED, BOT_GTK_PARAM_WIDGET_SPINBOX,
                                                1,50, 1, 15); 

    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_MANIP_PLAN_INITSEED_MODE, 1, NULL);

                                                     
    bot_gtk_param_widget_add_enum(self->pw, PARAM_MANIP_PLAN_MODE, BOT_GTK_PARAM_WIDGET_MENU,drc::manip_plan_control_t::IKSEQUENCE_ON, 
                                       "IkSequenceOn", drc::manip_plan_control_t::IKSEQUENCE_ON,
                                       "IkSequenceOff", drc::manip_plan_control_t::IKSEQUENCE_OFF,
                                       "Teleop", drc::manip_plan_control_t::TELEOP,
                                       "Fixed Joints", drc::manip_plan_control_t::FIXEDJOINTS,
                                       NULL);

    bot_gtk_param_widget_add_booleans(self->pw, BOT_GTK_PARAM_WIDGET_CHECKBOX, PARAM_PLAN_USING_BDI_HEIGHT_MODE, 1, NULL);
    
    bot_gtk_param_widget_add_buttons(self->pw, PARAM_UPDATE_PLANNER_PARAMS, NULL);
    // don't publish these on launch: 
    // update_planar_params(self);


        
   	g_signal_connect(G_OBJECT(self->pw), "changed", G_CALLBACK(on_param_widget_changed), self);
  	self->selection_enabled = 1;

    // off by default. only ever turned on for drilling (dec 2013, mfallon):
    bot_gtk_param_widget_set_bool(self->pw, PARAM_PLAN_USING_BDI_HEIGHT_MODE,false); 
    bot_gtk_param_widget_set_bool(self->pw, PARAM_SELECTION,self->selection_enabled);
    self->use_colormap = 1; // default - never changed now
    //bot_gtk_param_widget_set_bool(self->pw, PARAM_USE_COLORMAP,self->use_colormap);
    self->clicked = 0;	
    self->dragging = 0;    
  	self->selection = new std::string(" ");
    self->marker_selection = new std::string(" ");  
    self->trigger_source_otdf_id = new std::string(" "); 
    self->marker_choice_state = HANDS;
    self->is_left_in_motion =  true;
    self->visualize_bbox = false;
    self->multiapprove_plan_execution_dock= NULL; 
    self->plan_execution_dock= NULL; 
    self->ignore_plan_execution_warning = FALSE;
    self->plan_approval_dock= NULL; 
    self->plan_execute_button= NULL; 
    self->afftriggered_popup = NULL;
    self->selected_plan_index= -1;
    self->selected_keyframe_index = -1;
    self->atlas_state = drc::atlas_status_t::BEHAVIOR_STAND;
    self->atlas_status_utime = 0; 
    self->_renderer_foviate = false;
    int plan_size =   self->robotPlanListener->_gl_robot_list.size();
    self->show_fullplan = bot_gtk_param_widget_get_bool(self->pw, PARAM_SHOW_FULLPLAN);
    self->show_keyframes = bot_gtk_param_widget_get_bool(self->pw, PARAM_SHOW_KEYFRAMES);
    double plan_part = bot_gtk_param_widget_get_double(self->pw, PARAM_PLAN_PART);
    if ((self->show_fullplan)||(plan_size==0)){
      self->displayed_plan_index = -1;
    }else{
      uint w_plan = (uint) round(plan_part* (plan_size -1));
      self->displayed_plan_index = w_plan;
    }
  
    //bot_viewer_add_renderer(viewer, &self->renderer, render_priority);
    bot_viewer_add_renderer_on_side(viewer,&self->renderer, render_priority, 0);
        
    BotEventHandler *ehandler = &self->ehandler;
    ehandler->name = (char*) RENDERER_NAME;
    if (operation_mode==1){
      ehandler->name =(char *) "Robot Plan Loopback";
    }else if(operation_mode==2){
      ehandler->name =(char *) "Robot Plan LB Compressed";
    }
    ehandler->enabled = 1;
    ehandler->pick_query = pick_query;
    ehandler->hover_query = NULL;
    ehandler->mouse_press = mouse_press;
    ehandler->mouse_release = mouse_release;
    ehandler->mouse_motion = mouse_motion;
    ehandler->user = self;
    
    bot_viewer_add_event_handler(viewer, &self->ehandler, render_priority);
   
}

void RendererRobotPlan::setPlanVisibility(const bool vis) {
    bot_gtk_param_widget_set_bool(this->pw, PARAM_HIDE, vis ? FALSE : TRUE);
}

bool RendererRobotPlan::getPlanVisibility() const {
    return (bot_gtk_param_widget_get_bool(this->pw, PARAM_HIDE) == 0);
}

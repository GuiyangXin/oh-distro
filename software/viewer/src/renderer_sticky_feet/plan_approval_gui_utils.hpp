#ifndef PLAN_APPROVAL_GUI_UTILS_HPP
#define PLAN_APPROVAL_GUI_UTILS_HPP

#include "renderer_sticky_feet.hpp"

using namespace std;
using namespace boost;
using namespace renderer_sticky_feet;
namespace renderer_sticky_feet_gui_utils
{
   
  static gboolean on_plan_cancel_button_clicked (GtkButton* button, void *user)
  {
    RendererStickyFeet *self = (RendererStickyFeet*) user;
    cout <<"Publishing on REJECTED_FOOTSTEP_PLAN" << endl;
    string channel = "REJECTED_FOOTSTEP_PLAN";   
    self->footStepPlanListener->commit_footstep_plan(self->robot_utime,channel);    
    self->footStepPlanListener->_gl_planned_stickyfeet_list.clear();    
    gtk_widget_destroy (self->plan_approval_dock);
    self->plan_approval_dock= NULL;
    self->footStepPlanListener->_waiting_for_new_plan = true;
    bot_viewer_request_redraw(self->viewer);
   return TRUE;
  }
  
  static  gboolean on_plan_approve_button_clicked (GtkButton* button, void *user)
  {
    RendererStickyFeet *self = (RendererStickyFeet*) user;
    cout <<"Foot step plan approved" << endl;
    cout <<"Publishing on APPROVED_FOOTSTEP_PLAN" << endl;
    string channel = "APPROVED_FOOTSTEP_PLAN";
    self->footStepPlanListener->commit_footstep_plan(self->robot_utime,channel);
    self->footStepPlanListener->_last_plan_approved_or_executed = true;
    //gtk_widget_destroy (self->plan_approval_dock);
    //self->plan_approval_dock= NULL;
    // self->footStepPlanListener->_waiting_for_new_plan = true;
    bot_viewer_request_redraw(self->viewer);
    return TRUE;
  }
   
  static void spawn_plan_approval_dock (void *user)
  {
  
   RendererStickyFeet *self = (RendererStickyFeet*) user;

/*  GtkWidget *window; 
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(self->viewer->window));
    gtk_window_set_modal(GTK_WINDOW(window), FALSE);
    gtk_window_set_decorated  (GTK_WINDOW(window),FALSE);
    gtk_window_stick(GTK_WINDOW(window));
    //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_NONE);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
    gtk_window_set_default_size(GTK_WINDOW(window), 100, 50);
    gint pos_x, pos_y;
    gtk_window_get_position(GTK_WINDOW(window),&pos_x,&pos_y);
  
    gint root_x, root_y;
    gtk_window_get_position (GTK_WINDOW(self->viewer->window), &root_x, &root_y);
    
    gint width, height;
    gtk_window_get_size(GTK_WINDOW(self->viewer->window),&width,&height);
    
    pos_x=root_x+0.5*width;    pos_y=root_y+0.25*height;
   // gint gdk_screen_height  (void);//Returns the height of the default screen in pixels.
    gtk_window_move(GTK_WINDOW(window),pos_x,pos_y);

    gtk_window_set_title(GTK_WINDOW(window), "Footstep Plan Approval Dock");
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
   */
    
    GtkWidget  *approve_button, *cancel_button;
    approve_button = (GtkWidget *) gtk_tool_button_new_from_stock(GTK_STOCK_APPLY);
    gtk_widget_set_size_request(approve_button, 70, 70);
    cancel_button = (GtkWidget *) gtk_tool_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_widget_set_size_request(cancel_button, 70, 70);
        
    gtk_widget_set_tooltip_text (approve_button, "Accept Footstep Plan");
    gtk_widget_set_tooltip_text (cancel_button, "Cancel Footstep Plan");
   
    
   //self->plan_approval_dock = window;    
   g_signal_connect (G_OBJECT (cancel_button),
                  "clicked",
                  G_CALLBACK (on_plan_cancel_button_clicked),
                  self);

   g_signal_connect (G_OBJECT (approve_button),
                  "clicked",
                  G_CALLBACK (on_plan_approve_button_clicked),
                  self);
    GtkWidget *hbox;
    hbox = gtk_hbox_new (FALSE, 0);
    //GtkWidget * sep = gtk_vseparator_new ();  
    GtkToolItem * sep = gtk_separator_tool_item_new ();
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (sep), FALSE, TRUE,10);
    GtkWidget * label = gtk_label_new ("Step Plan:");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE,0);
    gtk_box_pack_start (GTK_BOX (hbox), approve_button, FALSE, FALSE,0);
    gtk_box_pack_start (GTK_BOX (hbox), cancel_button, FALSE, FALSE,0);


  GtkToolItem * toolitem = gtk_tool_item_new ();   
  gtk_container_add (GTK_CONTAINER (toolitem), hbox);   


  gtk_toolbar_insert (GTK_TOOLBAR (self->viewer->toolbar), toolitem, 9);
  self->plan_approval_dock = GTK_WIDGET(toolitem);  
  std::cout << "running gtk_widget_show_all in plan approval gui spawn\n";
  gtk_widget_show_all (self->plan_approval_dock);

    //gtk_container_add (GTK_CONTAINER (window), hbox);
    //gtk_widget_show_all(window);
    //gtk_window_set_focus(GTK_WINDOW(window), NULL);
  }

  
  
  
}
#endif

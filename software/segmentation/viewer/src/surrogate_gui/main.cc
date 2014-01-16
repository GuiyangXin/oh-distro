#include <string.h>

#include <bot_core/bot_core.h>
#include <bot_vis/bot_vis.h>
#include <gtk/gtk.h>
#include <lcm/lcm.h>
#include <bot_lcmgl_render/lcmgl_bot_renderer.h>

/*#include <bot_vis/viewer.h>
#include <bot_vis/bot_vis.h>
#include <bot_vis/param_widget.h>
#include <bot_vis/glm.h>
#include <bot_frames/bot_frames.h>
#include <bot_param/param_client.h>
#include <bot_param/param_util.h>
*/


#include "UIProcessing.h"
#include "LinearAlgebra.h"
#include "perception/PclSurrogateUtils.h"
#include <renderer_robot_plan/renderer_robot_plan.hpp>

// new grid renderer:
#include <renderer_drc/renderer_drcgrid.h>

//#include <otdf_renderer/renderer_otdf.hpp>
#include <renderer_affordances/renderer_affordances.hpp>
#include <visualization_utils/keyboard_signal_utils.hpp>
#include <ConciseArgs>

using namespace surrogate_gui;

typedef struct {
    BotViewer *viewer;
    lcm_t *lcm;
} state_t;

// NOTE (Sisir, 8th Jul 13): We dont want to have individual keyboard event handlers in renderers
// as designed in lib-bot as the key events can be non-unique and the key handlers can conflict. 
// Using boost::signals to create a global keyboard signal. Each renderer creates a corresponding 
// slot to handle global key events. Using a shared ptr to the boost signal. Renderers that require access
// to keyboard signals will receive the signal ref as an argument in their setup function.
KeyboardSignalRef _keyboardSignalRef = KeyboardSignalRef(new KeyboardSignal()); 

static int
on_key_press(BotViewer *viewer, BotEventHandler *ehandler,
        const GdkEventKey *event)
{
    int keyval = event->keyval;
    //std::cout << "keyval: " << keyval << "\n";
    
    // emit global keyboard signal, second argument indicates that it is a keypress (if true)
    (*_keyboardSignalRef)(keyval,true); 
   
    return 1;
}

static int
on_key_release(BotViewer *viewer, BotEventHandler *ehandler,
        const GdkEventKey *event)
{
    int keyval = event->keyval;
    // emit global keyboard signal, second argument indicates that it is a keyrelease (if false)
    (*_keyboardSignalRef)(keyval,false); // emit global keyboard signal
    
    switch (keyval)
    {
      case SHIFT_L:
      case SHIFT_R:
          //std::cout << "shift released\n";
          break;
      default:
          return 0;
    }

    return 1;
}


/////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  
  string role = "robot";
  ConciseArgs opt(argc, (char**)argv);
  opt.add(role, "r", "role","Role - robot or base");
  opt.parse();
  std::cout << "role: " << role << "\n";
  

  string lcm_url="";
  std::string role_upper;
  for(short i = 0; i < role.size(); ++i)
     role_upper+= (std::toupper(role[i]));
  if((role.compare("robot") == 0) || (role.compare("base") == 0) ){
    for(short i = 0; i < role_upper.size(); ++i)
       role_upper[i] = (std::toupper(role_upper[i]));
    string env_variable_name = string("LCM_URL_DRC_" + role_upper); 
    char* env_variable;
    env_variable = getenv (env_variable_name.c_str());
    if (env_variable!=NULL){
      //printf ("The env_variable is: %s\n",env_variable);      
      lcm_url = string(env_variable);
    }else{
      std::cout << env_variable_name << " environment variable has not been set ["<< lcm_url <<"]\n";     
      exit(-1);
    }
  }else{
    std::cout << "Role not understood, choose: robot or base\n";
    return 1;
  } 


  string vis_config_file="";
  if(role.compare("robot") == 0){
     vis_config_file = ".bot-plugin-robot-segmentation-viewer";
  }else if(role.compare("base") == 0){  
     vis_config_file = ".bot-plugin-base-segmentation-viewer";
  }else{
    std::cout << "DRC Viewer role not understood, choose: robot or base\n";
    return 1;
  }  
  
	//LinearAlgebra::runTests();
	//PclSurrogateUtils::runTests();
	//if (true) return 1;

	/*for (int i = 0; i < Color::HIGHLIGHT; i++)
	{
		cout << " i = " << i << " getColor(i) = " <<
				Color::getColor(i) << " | as int = " <<
				((int) Color::getColor(i)) << endl;
	}

	vector<Color::StockColor> colors = Color::getDifferentColors(Color::BLUE, 5);
	cout << endl << "===different colors w/ initial = " << Color::BLUE << "====" << endl;
	for (uint i = 0; i < colors.size(); i++)
	{
		cout << "\nnext color = " << colors[i] << endl;
	}


	return 0;
*/
    gtk_init(&argc, &argv);
    glutInit(&argc, argv);
    g_thread_init(NULL);

    setlinebuf(stdout);

    state_t app;
    memset(&app, 0, sizeof(app));

    BotViewer *viewer = bot_viewer_new("Segmentation");
    app.viewer = viewer;
    app.lcm = lcm_create( lcm_url.c_str() );
    BotParam* bot_param = bot_param_new_from_server(app.lcm, 0);
    if (bot_param == NULL) {
      fprintf(stderr, "Couldn't get bot param from server.\n");
      return 1;
    }
    BotFrames* bot_frames = bot_frames_new(app.lcm, bot_param);
    boost::shared_ptr<lcm::LCM> lcmCpp = boost::shared_ptr<lcm::LCM>(new lcm::LCM(app.lcm));
    
    bot_glib_mainloop_attach_lcm(app.lcm);

    //otdf
    // older: setup_renderer_otdf(viewer, 0, lcmCpp);

  // logplayer controls
  BotEventHandler *ehandler = (BotEventHandler*) calloc(1, sizeof(BotEventHandler));
  ehandler->name = "LogPlayer Remote";
  ehandler->enabled = 1;
  ehandler->key_press = on_key_press;
  ehandler->key_release = on_key_release;
  bot_viewer_add_event_handler(viewer, ehandler, 0);

 
  setup_renderer_affordances(viewer, 0, lcmCpp->getUnderlyingLCM(), bot_frames,_keyboardSignalRef);

    // setup renderers
    drcgrid_add_renderer_to_viewer(viewer, 1, lcmCpp->getUnderlyingLCM());
    // bot_viewer_add_stock_renderer(viewer, BOT_VIEWER_STOCK_RENDERER_GRID, 1);
    //KinectRendererXYZRGB *krxyzrgb = kinect_add_renderer_xyzrgb_to_viewer(viewer, 0,NULL,NULL);

    // lcmgl
    bot_lcmgl_add_renderer_to_viewer(viewer, lcmCpp->getUnderlyingLCM(), 1);

	// create segmentation handler
    std::string channel_name = "LOCAL_MAP_POINTS";
    UIProcessing uip(viewer, lcmCpp, channel_name.c_str()); //"KINECT_XYZRGB_ROS");

    std::cout << "Listening to data from channel: " << channel_name << "\n";

    // load saved preferences
    char *fname = g_build_filename(g_get_user_config_dir(), vis_config_file.c_str() , NULL);
    bot_viewer_load_preferences(viewer, fname);

    printf("\n\n\n========USAGE:\n");
    printf("'a' = display axes for currently force or rotation-axis vector [track mode]\n");
    printf("'c' = camera move\n");
    printf("'g' = display trajector determined by force vector + rotation axis\n");
    printf("'r' = rectangle select (valid in selection mode)\n");
    printf("'s' = turn segment coloring on/off (valid in selection mode)\n");
    printf("'t' = turn tracking info on/off\n");
    printf("'u' = turn model fit display on/off in segmenting mode\n");
    printf("'<--' and '-->' switch between sub-components of an object [segment mode]");
    printf("'<--', '-->', [up|down] arrows +- [shift_l and/or shift_r]: rotate vector [track mode]");
    printf("'tab', switch between rotation axis vector and force vector [track mode]");

    printf("\n==============\n\n\n");

    // run the main loop
    gtk_main();

    // save any changed preferences
    bot_viewer_save_preferences(viewer, fname);
    free(fname);

    // cleanup
    bot_viewer_unref(viewer);

    return 0;
}

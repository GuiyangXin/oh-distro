
#include <iostream>


#include <leg-odometry/TwoLegOdometry.h>
#include <leg-odometry/LegOdometry_LCM_Handler.hpp>
#include <csignal>
#include <exception>
#include <ConciseArgs>
#include <unistd.h>


using namespace std;



//TwoLegs::TwoLegOdometry* _leg_odo; // excessive, as this gets tested in _legs_motion_estimate
LegOdometry_Handler* _legs_motion_estimate;

void signalHandler( int signum ){
  cout << "Interrupt signal (" << signum << ") received.\n";

  // cleanup and close up 
  try{
    _legs_motion_estimate->terminate();
  } catch (std::exception &e){
    std::cout << "Exception occurred during close out\n";
  }
  // terminate program  

  exit(signum);  

}

int main(int argc, char ** argv) {
  command_switches switches;
  
  switches.publish_footcontact_states = false;
  switches.do_estimation = false;
  switches.draw_footsteps = false;
  switches.log_data_files = false;
  switches.lcm_add_ext = false;
  switches.lcm_read_trues = false;
  switches.use_true_z = false;
  switches.print_computation_time = false;
  switches.OPTION_A = false;
  switches.OPTION_B = false;
  switches.OPTION_C = false;
  switches.OPTION_D = false;
  switches.OPTION_E = false;
  switches.grab_true_init = false;
  switches.verbose = false;
  switches.slide_compensation = false;
  switches.medianlength=5;

  ConciseArgs opt(argc, (char**)argv);
  opt.add(switches.do_estimation, "e", "do_estimation","Do motion estimation");
  opt.add(switches.draw_footsteps, "f", "draw_footsteps","Draw footstep poses in viewer");
  opt.add(switches.log_data_files, "l", "log_data_files","Logging some data to file");
  opt.add(switches.lcm_add_ext, "x", "lcm_add_ext", "Adding extension to the LCM messages");
  opt.add(switches.lcm_read_trues, "t", "lcm_read_trues", "Listening to true robot states, including POSE_HEAD");
  opt.add(switches.use_true_z,"z","Use true z position data for state estimate");
  opt.add(switches.print_computation_time,"i", "Print the measured computation time to screen");
  opt.add(switches.OPTION_A,"A","MedianFitler -> Dist Diff -> Rate decimation");
  opt.add(switches.OPTION_B,"B","Dist Diff -> MedianFitler -> Rate decimation");
  opt.add(switches.OPTION_C,"C","Dist Diff -> Rate decimation -> MedianFitler");
  opt.add(switches.OPTION_D,"D","BlipFilter -> Dist Diff -> MedianFitler -> Rate decimation");
  opt.add(switches.OPTION_E,"E","BlipFilter -> Dist Diff -> Rate decimation -> MedianFitler");
  opt.add(switches.grab_true_init,"y","Initializing the state with TRUE_ROBOT_STATE message");
  opt.add(switches.medianlength, "m", "Casually challenged median filter length");
  opt.add(switches.verbose,"v","Enable verbose debug printouts");
  opt.add(switches.slide_compensation,"s","Incorporate foot sliding compensation");
  opt.parse();
  std::cout << "Do motion estimation: " << switches.do_estimation<< std::endl;
  std::cout << "Draw footsteps: " << switches.draw_footsteps << std::endl;
  std::cout << "Logging of data to file: " << switches.log_data_files << std::endl;
  std::cout << "Adding extension to LCM messages: " << switches.lcm_add_ext << std::endl;
  std::cout << "Listening to truth LCM messages, including POSE_HEAD: " << switches.lcm_read_trues << std::endl;
  std::cout << "Using true Z position for state estimate: " << switches.use_true_z << std::endl;

  // register signal SIGINT and signal handler  
  signal(SIGINT, signalHandler);  

  cout << "Test driver main function for the twoleg motion estimate pod" << endl;

  //nice(-20);

  boost::shared_ptr<lcm::LCM> lcm(new lcm::LCM);
  if(!lcm->good())
    return 1;

  //_leg_odo = new TwoLegs::TwoLegOdometry(); // This is excessive, as the class is also invoked by LegOdometry_Handler() object.
  _legs_motion_estimate = new LegOdometry_Handler(lcm, &switches);


  // Do some stuff with the objects to test them. Preferably here you must call the internal testing functions of the different objects created..
  //_legs_motion_estimate->run(false); // true means it will operate in testing mode and not listen LCM messages

  while(0 == lcm->handle());

  //delete _leg_odo;
  delete _legs_motion_estimate;

  cout << "Everything ends in test_driver for legs_motion_estimate program" << endl; 

  return 0;
}


// here somewhere we need to listen to the LCM packets and call back the function in the TwoLegOdometry class as to update its internal states

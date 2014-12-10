#include <iostream>
#include <Eigen/Dense>
#include <collision/collision_object_gfe.h>

using namespace std;
using namespace Eigen;
using namespace drc;
using namespace kinematics;
using namespace collision;

int 
main( int argc, 
      char** argv )
{
  cout << endl << "start of collision-object-gfe-test" << endl << endl;
  // create a collision object class
  Collision_Object_GFE collision_object_gfe( "gfe1" );

  robot_state_t robot_state;
  robot_state.pose.translation.x = 0.0;
  robot_state.pose.translation.y = 0.0;
  robot_state.pose.translation.z = 1.0;
  robot_state.pose.rotation.x = 0.0;
  robot_state.pose.rotation.y = 0.0;
  robot_state.pose.rotation.z = 0.0;
  robot_state.pose.rotation.w = 1.0;
  robot_state.num_joints = collision_object_gfe.kinematics_model().model().joints_.size();
  robot_state.joint_name.resize( collision_object_gfe.kinematics_model().model().joints_.size() );
  unsigned int index = 0;
  for ( std::map< std::string, boost::shared_ptr< urdf::Joint > >::const_iterator it = collision_object_gfe.kinematics_model().model().joints_.begin(); it != collision_object_gfe.kinematics_model().model().joints_.end(); it++ ){
    robot_state.joint_name[ index ] = it->first;
    index++;
  }

  robot_state.joint_position.resize( collision_object_gfe.kinematics_model().model().joints_.size() );
  for( unsigned int i = 0; i < collision_object_gfe.kinematics_model().model().joints_.size(); i++ ){
    robot_state.joint_position[ i ] = 0.0;
  }

  collision_object_gfe.set( robot_state );

  cout << "collision_object_gfe: " << collision_object_gfe << endl;

  cout << endl << "end of collision-object-gfe-test" << endl << endl;
  return 0;
}

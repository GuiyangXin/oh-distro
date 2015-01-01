#include <path_util/path_util.h>
#include <collision/collision_object_box.h>
#include <collision/collision_object_cylinder.h>
#include <collision/collision_object_sphere.h>
#include <collision/collision_object_convex_hull.h>
#include <collision/collision_object_gfe.h>

using namespace std;
using namespace boost;
using namespace Eigen;
using namespace urdf;
using namespace KDL;
using namespace drc;
using namespace state;
using namespace kinematics;
using namespace collision;

/**
 * Collision_Object_GFE
 * class constructor
 */
Collision_Object_GFE::
Collision_Object_GFE( string id,
                      Atlas_Version atlas_version,
                      collision_object_gfe_collision_object_type_t collisionObjectType ) : Collision_Object( id ),
                                                                                            _collision_objects(),
                                                                                            _kinematics_model(atlas_version) {
  _load_collision_objects( collisionObjectType );
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      for( unsigned int j = 0; j < _collision_objects[ i ]->bt_collision_objects().size(); j++ ){
        _bt_collision_objects.push_back( _collision_objects[ i ]->bt_collision_objects()[ j ] );
      }
    }
  }
  State_GFE state_gfe;
  set( state_gfe );
}

/**
 * Collision_Object_GFE
 * class constructor with id and urdf filename
 */
Collision_Object_GFE::
Collision_Object_GFE( string id,
                      string xmlString,
                      Atlas_Version atlas_version,
                      collision_object_gfe_collision_object_type_t collisionObjectType ) : Collision_Object( id ),
                                                                                          _collision_objects(),
                                                                                          _kinematics_model( xmlString, atlas_version ){
  _load_collision_objects( collisionObjectType );
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      for( unsigned int j = 0; j < _collision_objects[ i ]->bt_collision_objects().size(); j++ ){
        _bt_collision_objects.push_back( _collision_objects[ i ]->bt_collision_objects()[ j ] );
      }
    }
  }
  State_GFE state_gfe;
  set( state_gfe );
} 

/**
 * Collision_Object_GFE
 * class constructor with id and urdf filename
 */
Collision_Object_GFE::
Collision_Object_GFE( string id,
                      string xmlString,
                      collision_object_gfe_collision_object_type_t collisionObjectType,
                      Atlas_Version atlas_version ) : Collision_Object( id ),
                                                      _collision_objects(),
                                                      _kinematics_model( xmlString, atlas_version ){
  Collision_Object_GFE(id, xmlString, atlas_version, collisionObjectType);
}
/**
 * Collision_Object_GFE
 * copy constructor
 */
Collision_Object_GFE::
Collision_Object_GFE( const Collision_Object_GFE& other ): Collision_Object( other ),
                                                            _collision_objects( other._collision_objects ),
                                                            _kinematics_model( other._kinematics_model ) {
  _load_collision_objects();
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      for( unsigned int j = 0; j < _collision_objects[ i ]->bt_collision_objects().size(); j++ ){
        _bt_collision_objects.push_back( _collision_objects[ i ]->bt_collision_objects()[ j ] );
      }
    }
  }
}   

/**
 * ~Collision_Object_GFE
 * class destructor
 */
Collision_Object_GFE::
~Collision_Object_GFE(){
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      delete _collision_objects[ i ];
      _collision_objects[ i ] = NULL;
    }
  }
  _collision_objects.clear();
}

/** 
 * set
 * sets the kinematics model to the robot state
 */
void
Collision_Object_GFE::
set( robot_state_t& robotState ){
  _kinematics_model.set( robotState );
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      Frame frame = _kinematics_model.link( _collision_objects[ i ]->id() );
      _collision_objects[ i ]->set_transform( frame );
    }
  }
  return;
}

/**
 * set
 * sets the kinematics model to the robot state
 */
void
Collision_Object_GFE::
set( State_GFE& stateGFE ){
  _kinematics_model.set( stateGFE );
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      Frame frame = _kinematics_model.link( _collision_objects[ i ]->id() );
      _collision_objects[ i ]->set_transform( frame );
    }
  }
  return;
}

/**
 * matches_uid
 */
Collision_Object*
Collision_Object_GFE::
matches_uid( unsigned int uid ){
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    vector< btCollisionObject* > bt_collision_object_vector = _collision_objects[ i ]->bt_collision_objects();
    for( unsigned int j = 0; j < bt_collision_object_vector.size(); j++ ){
      if( bt_collision_object_vector[ j ]->getBroadphaseHandle()->getUid() == uid ){
        return _collision_objects[ i ];
      }
    }
  }
  return NULL;
}

/**
 * kinematics_model
 * returns a reference to the Kinematics_Model class
 */
const Kinematics_Model_GFE&
Collision_Object_GFE::
kinematics_model( void )const{
  return _kinematics_model;
}

/**
 * _load_collision_objects
 * iterates through all of the links and loads collision objects based on the link type
 */
void
Collision_Object_GFE::
_load_collision_objects( collision_object_gfe_collision_object_type_t collisionObjectType ){
  vector< shared_ptr< Link > > links;
  _kinematics_model.model().getLinks( links );
  string models_path = getModelsPath();

  if( collisionObjectType == COLLISION_OBJECT_GFE_COLLISION_OBJECT_COLLISION ){
    for( unsigned int i = 0; i < links.size(); i++ ){
      for( std::map< std::string, boost::shared_ptr<std::vector<boost::shared_ptr<Collision> > > >::iterator it = links[i]->collision_groups.begin(); it != links[i]->collision_groups.end(); it++ ){
        for( unsigned int j = 0; j < it->second->size(); j++ ){
          if( (*it->second)[ j ] != NULL ){
            if( (*it->second)[ j ]->geometry->type == Geometry::SPHERE ){
              shared_ptr< Sphere > sphere = dynamic_pointer_cast< Sphere >( (*it->second)[ j ]->geometry );
              _collision_objects.push_back( new Collision_Object_Sphere( links[ i ]->name, sphere->radius, Frame( KDL::Rotation::Quaternion( (*it->second)[ j ]->origin.rotation.x, (*it->second)[ j ]->origin.rotation.y, (*it->second)[ j ]->origin.rotation.z, (*it->second)[ j ]->origin.rotation.w ), KDL::Vector( (*it->second)[ j ]->origin.position.x, (*it->second)[ j ]->origin.position.y, (*it->second)[ j ]->origin.position.z ) ) ) );
              cout << *_collision_objects.back() << endl; 
            } else if ( (*it->second)[ j ]->geometry->type == Geometry::BOX ){
              shared_ptr< Box > box = dynamic_pointer_cast< Box >( (*it->second)[ j ]->geometry );
              _collision_objects.push_back( new Collision_Object_Box( links[ i ]->name, Vector3f( box->dim.x, box->dim.y, box->dim.z ), Frame( KDL::Rotation::Quaternion( (*it->second)[ j ]->origin.rotation.x, (*it->second)[ j ]->origin.rotation.y, (*it->second)[ j ]->origin.rotation.z, (*it->second)[ j ]->origin.rotation.w ), KDL::Vector( (*it->second)[ j ]->origin.position.x, (*it->second)[ j ]->origin.position.y, (*it->second)[ j ]->origin.position.z ) ) ) );
              cout << *_collision_objects.back() << endl; 
            } else if ( (*it->second)[ j ]->geometry->type == Geometry::CYLINDER ){
              shared_ptr< Cylinder > cylinder = dynamic_pointer_cast< Cylinder >( (*it->second)[ j ]->geometry );
              _collision_objects.push_back( new Collision_Object_Cylinder( links[ i ]->name, cylinder->radius, cylinder->length, Frame( KDL::Rotation::Quaternion( (*it->second)[ j ]->origin.rotation.x, (*it->second)[ j ]->origin.rotation.y, (*it->second)[ j ]->origin.rotation.z, (*it->second)[ j ]->origin.rotation.w ), KDL::Vector( (*it->second)[ j ]->origin.position.x, (*it->second)[ j ]->origin.position.y, (*it->second)[ j ]->origin.position.z ) ) ) );
              cout << *_collision_objects.back() << endl; 
            } else if ( (*it->second)[ j ]->geometry->type == Geometry::MESH ) {
              shared_ptr< Mesh > mesh = dynamic_pointer_cast< Mesh >( (*it->second)[ j ]->geometry );
              std::string model_filename = mesh->filename;
              model_filename.erase( model_filename.begin(), model_filename.begin() + 9 );
              model_filename.erase( model_filename.end() - 4, model_filename.end() );
              model_filename = models_path + string( "/mit_gazebo_models" ) + model_filename + string( "_chull.obj" );
              _collision_objects.push_back( new Collision_Object_Convex_Hull( links[ i ]->name, model_filename ) );
              cout << *_collision_objects.back() << endl;
            }
          }
        }
      }

      if( links[ i ]->collision != NULL ){
        if( links[ i ]->collision->geometry->type == Geometry::SPHERE ){
          shared_ptr< Sphere > sphere = dynamic_pointer_cast< Sphere >( links[ i ]->collision->geometry );
          _collision_objects.push_back( new Collision_Object_Sphere( links[ i ]->name, sphere->radius, Frame( KDL::Rotation::Quaternion( links[ i ]->collision->origin.rotation.x, links[ i ]->collision->origin.rotation.y, links[ i ]->collision->origin.rotation.z, links[ i ]->collision->origin.rotation.w ), KDL::Vector( links[ i ]->collision->origin.position.x, links[ i ]->collision->origin.position.y, links[ i ]->collision->origin.position.z ) ) ) );
          cout << *_collision_objects.back() << endl;
        } else if ( links[ i ]->collision->geometry->type == Geometry::BOX ){
          shared_ptr< Box > box = dynamic_pointer_cast< Box >( links[ i ]->collision->geometry );
          _collision_objects.push_back( new Collision_Object_Box( links[ i ]->name, Vector3f( box->dim.x, box->dim.y, box->dim.z ), Frame( KDL::Rotation::Quaternion( links[ i ]->collision->origin.rotation.x, links[ i ]->collision->origin.rotation.y, links[ i ]->collision->origin.rotation.z, links[ i ]->collision->origin.rotation.w ), KDL::Vector( links[ i ]->collision->origin.position.x, links[ i ]->collision->origin.position.y, links[ i ]->collision->origin.position.z ) ) ) );
          cout << *_collision_objects.back() << endl;
        } else if ( links[ i ]->collision->geometry->type == Geometry::CYLINDER ){
          shared_ptr< Cylinder > cylinder = dynamic_pointer_cast< Cylinder >( links[ i ]->collision->geometry );
          _collision_objects.push_back( new Collision_Object_Cylinder( links[ i ]->name, cylinder->radius, cylinder->length, Frame( KDL::Rotation::Quaternion( links[ i ]->collision->origin.rotation.x, links[ i ]->collision->origin.rotation.y, links[ i ]->collision->origin.rotation.z, links[ i ]->collision->origin.rotation.w ), KDL::Vector( links[ i ]->collision->origin.position.x, links[ i ]->collision->origin.position.y, links[ i ]->collision->origin.position.z ) ) ) );
          cout << *_collision_objects.back() << endl;
        } else if ( links[ i ]->collision->geometry->type == Geometry::MESH ){
          shared_ptr< Mesh > mesh = dynamic_pointer_cast< Mesh >( links[ i ]->collision->geometry );
          std::string model_filename = mesh->filename;
          model_filename.erase( model_filename.begin(), model_filename.begin() + 9 );
          model_filename.erase( model_filename.end() - 4, model_filename.end() );
          model_filename = models_path + string( "/mit_gazebo_models" ) + model_filename + string( "_chull.obj" );
          _collision_objects.push_back( new Collision_Object_Convex_Hull( links[ i ]->name, model_filename ) );      
          cout << *_collision_objects.back() << endl; 
        } 
      }
    }
  } else if ( collisionObjectType == COLLISION_OBJECT_GFE_COLLISION_OBJECT_VISUAL ){
    for( unsigned int i = 0; i < links.size(); i++ ){
      if( links[ i ]->visual != NULL ){
        if( links[ i ]->visual->geometry->type == Geometry::SPHERE ){
          shared_ptr< Sphere > sphere = dynamic_pointer_cast< Sphere >( links[ i ]->visual->geometry );
          _collision_objects.push_back( new Collision_Object_Sphere( links[ i ]->name, sphere->radius, Frame( KDL::Rotation::Quaternion( links[ i ]->visual->origin.rotation.x, links[ i ]->visual->origin.rotation.y, links[ i ]->visual->origin.rotation.z, links[ i ]->visual->origin.rotation.w ), KDL::Vector( links[ i ]->visual->origin.position.x, links[ i ]->visual->origin.position.y, links[ i ]->visual->origin.position.z ) ) ) );
          cout << *_collision_objects.back() << endl;
        } else if ( links[ i ]->visual->geometry->type == Geometry::BOX ){
          shared_ptr< Box > box = dynamic_pointer_cast< Box >( links[ i ]->visual->geometry );
          _collision_objects.push_back( new Collision_Object_Box( links[ i ]->name, Vector3f( box->dim.x, box->dim.y, box->dim.z ), Frame( KDL::Rotation::Quaternion( links[ i ]->visual->origin.rotation.x, links[ i ]->visual->origin.rotation.y, links[ i ]->visual->origin.rotation.z, links[ i ]->visual->origin.rotation.w ), KDL::Vector( links[ i ]->visual->origin.position.x, links[ i ]->visual->origin.position.y, links[ i ]->visual->origin.position.z ) ) ) );
          cout << *_collision_objects.back() << endl;
        } else if ( links[ i ]->visual->geometry->type == Geometry::CYLINDER ){
          shared_ptr< Cylinder > cylinder = dynamic_pointer_cast< Cylinder >( links[ i ]->visual->geometry );
          _collision_objects.push_back( new Collision_Object_Cylinder( links[ i ]->name, cylinder->radius, cylinder->length, Frame( KDL::Rotation::Quaternion( links[ i ]->visual->origin.rotation.x, links[ i ]->visual->origin.rotation.y, links[ i ]->visual->origin.rotation.z, links[ i ]->visual->origin.rotation.w ), KDL::Vector( links[ i ]->visual->origin.position.x, links[ i ]->visual->origin.position.y, links[ i ]->visual->origin.position.z ) ) ) );
          cout << *_collision_objects.back() << endl;
        } else if ( links[ i ]->visual->geometry->type == Geometry::MESH ){
          shared_ptr< Mesh > mesh = dynamic_pointer_cast< Mesh >( links[ i ]->visual->geometry );
          std::string model_filename = mesh->filename;
          model_filename.erase( model_filename.begin(), model_filename.begin() + 9 );
          model_filename.erase( model_filename.end() - 4, model_filename.end() );
          model_filename = models_path + string( "/mit_gazebo_models" ) + model_filename + string( "_chull.obj" );
          _collision_objects.push_back( new Collision_Object_Convex_Hull( links[ i ]->name, model_filename ) );
          _collision_objects.back()->set_offset( Frame( KDL::Rotation::Quaternion( links[ i ]->visual->origin.rotation.x, links[ i ]->visual->origin.rotation.y, links[ i ]->visual->origin.rotation.z, links[ i ]->visual->origin.rotation.w ), KDL::Vector( links[ i ]->visual->origin.position.x, links[ i ]->visual->origin.position.y, links[ i ]->visual->origin.position.z ) ) );
          cout << *_collision_objects.back() << endl;
        }
      }
    }
  } else {
    cout << "could not interpret collision object type" << endl;
  } 
  return;
}

void Collision_Object_GFE::set_transform( const Eigen::Vector3f position, const Eigen::Vector4f orientation ) 
{
  throw std::runtime_error("Not Implemented: collision_object_gfe.cc --> set_transform");
}

void
Collision_Object_GFE::
set_transform( const Frame& transform ){
  throw std::runtime_error("Not Implemented: collision_object_gfe.cc --> set_transform");
}


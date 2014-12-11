#include <collision/collision_object_box.h>

using namespace std;
using namespace Eigen;
using namespace KDL;
using namespace collision;

/**
 * Collision_Object_Box
 * class constructor with id, dimension, position, and orientation arguments
 */
Collision_Object_Box::
Collision_Object_Box( string id,
                      Vector3f dims,
                      Vector3f position,
                      Vector4f orientation ) : Collision_Object( id ),
                                                _bt_collision_object(),
                                                _bt_box_shape( btVector3( dims.x()/2.0, dims.y()/2.0, dims.z()/2.0 ) ){
  set_transform( position, orientation );
  _bt_collision_object.setCollisionShape( &_bt_box_shape );
  _bt_collision_objects.push_back( &_bt_collision_object );
}

/**
 * Collision_Object_Box
 * class constructor with id, dimension, position, and orientation arguments
 */
Collision_Object_Box::
Collision_Object_Box( string id,
                      Vector3f dims,
                      const Frame& offset,
                      const Frame& transform ) : Collision_Object( id, true, offset ),
                                                _bt_collision_object(),
                                                _bt_box_shape( btVector3( dims.x()/2.0, dims.y()/2.0, dims.z()/2.0 ) ){
  set_transform( transform );
  _bt_collision_object.setCollisionShape( &_bt_box_shape );
  _bt_collision_objects.push_back( &_bt_collision_object );
}

/**
 * Collision_Object_Box
 * copy constructor 
 */
Collision_Object_Box::
Collision_Object_Box( const Collision_Object_Box& other ) : Collision_Object( other ),
                                                            _bt_collision_object(),
                                                            _bt_box_shape( btVector3( other.bt_box_shape().getImplicitShapeDimensions().x() + other.bt_box_shape().getMargin(), other.bt_box_shape().getImplicitShapeDimensions().y() + other.bt_box_shape().getMargin(), other.bt_box_shape().getImplicitShapeDimensions().z() + other.bt_box_shape().getMargin() ) ){
  _bt_collision_object.setCollisionShape( &_bt_box_shape );
}

/**
 * ~Collision_Object_Box
 * class destructor
 */
Collision_Object_Box::
~Collision_Object_Box(){

}

/** 
 * set_transform
 * sets the world-frame position and orientation of the collision object
 */
void
Collision_Object_Box::
set_transform( const Vector3f position,
                const Vector4f orientation ){
  _bt_collision_object.setWorldTransform( btTransform( btQuaternion( orientation.x(), orientation.y(), orientation.z(), orientation.w() ),
                                                        btVector3( position.x(), position.y(), position.z() ) ) );
  return;
}

void
Collision_Object_Box::
set_transform( const Frame& transform ){
  Frame origin = transform * _offset;
  double qx = 0.0;
  double qy = 0.0;
  double qz = 0.0;
  double qs = 0.0;
  origin.M.GetQuaternion( qx, qy, qz, qs );
  _bt_collision_object.setWorldTransform( btTransform( btQuaternion( qx, qy, qz, qs ), btVector3( origin.p[0], origin.p[1], origin.p[2] ) ) ); 
  return;
}

/**
 * bt_box_shape
 * return a reference to the bt_box_shape
 */
const btBoxShape&
Collision_Object_Box::
bt_box_shape( void )const{
  return _bt_box_shape;
}

namespace collision {
  ostream&
  operator<<( ostream& out,
              const Collision_Object_Box& other ){
    out << "id:{" << other.id().c_str() << "} ";
    out << "bt_collision_objects[" << other.bt_collision_objects().size() << "]:{";
    for( unsigned int i = 0; i < other.bt_collision_objects().size(); i++ ){
      if( other.bt_collision_objects()[ i ]->getBroadphaseHandle() != NULL ){
        out << other.bt_collision_objects()[ i ]->getBroadphaseHandle()->getUid();
      } else {
        out << "N/A";
      }
      out << ":{pos:(" << other.bt_collision_objects()[ i ]->getWorldTransform().getOrigin().x() << "," << other.bt_collision_objects()[ i ]->getWorldTransform().getOrigin().y() << "," << other.bt_collision_objects()[ i ]->getWorldTransform().getOrigin().z() << "),(" << other.bt_collision_objects()[ i ]->getWorldTransform().getRotation().getX() << "," << other.bt_collision_objects()[ i ]->getWorldTransform().getRotation().getY() << "," << other.bt_collision_objects()[ i ]->getWorldTransform().getRotation().getZ() << "," << other.bt_collision_objects()[ i ]->getWorldTransform().getRotation().getW() << ")}";
      out << ",box:(" << other.bt_box_shape().getImplicitShapeDimensions().x() + other.bt_box_shape().getMargin() << "," << other.bt_box_shape().getImplicitShapeDimensions().y() + other.bt_box_shape().getMargin() << "," << other.bt_box_shape().getImplicitShapeDimensions().z() + other.bt_box_shape().getMargin() << ")";
      if( i != ( other.bt_collision_objects().size() - 1 ) ){
        out << ",";
      }
    }
    out << "}";
    return out;
  }
}

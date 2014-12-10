#ifndef AUTHORING_OPENGL_OBJECT_CONSTRAINT_TASK_SPACE_REGION_H
#define AUTHORING_OPENGL_OBJECT_CONSTRAINT_TASK_SPACE_REGION_H

#include <iostream>

#include <opengl/opengl_object.h>
#include <opengl/opengl_object_box.h>

#include <authoring/constraint_task_space_region.h>
#include <collision/collision_detector.h>

namespace authoring {
  class OpenGL_Object_Constraint_Task_Space_Region: public opengl::OpenGL_Object {
  public:
    OpenGL_Object_Constraint_Task_Space_Region();
    ~OpenGL_Object_Constraint_Task_Space_Region();
    OpenGL_Object_Constraint_Task_Space_Region( const OpenGL_Object_Constraint_Task_Space_Region& other );
    OpenGL_Object_Constraint_Task_Space_Region& operator=( const OpenGL_Object_Constraint_Task_Space_Region& other );

    virtual void set( const Constraint_Task_Space_Region& constraint );
    virtual void set_transparency( double transparency );
    virtual void set_color( Eigen::Vector3f color );
    virtual void set_affordance( affordance::AffordanceState& affordanceState );  
  
    virtual void draw( void );
    virtual void add_to_collision( collision::Collision_Detector& detector );
  protected:
    opengl::OpenGL_Object_Box _opengl_object_box;
    affordance::AffordanceState _affordance_state;

  private:

  };
  std::ostream& operator<<( std::ostream& out, const OpenGL_Object_Constraint_Task_Space_Region& other );
}

#endif /* AUTHORING_OPENGL_OBJECT_CONSTRAINT_TASK_SPACE_REGION_H */

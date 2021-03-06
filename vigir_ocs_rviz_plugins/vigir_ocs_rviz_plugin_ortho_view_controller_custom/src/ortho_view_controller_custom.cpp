/*
 * Copyright (c) 2009, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <OGRE/OgreCamera.h>
#include <OGRE/OgreQuaternion.h>
#include <OGRE/OgreSceneManager.h>
#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreVector3.h>
#include <OGRE/OgreViewport.h>

#include "rviz/display_context.h"
#include "rviz/ogre_helpers/orthographic.h"
#include "rviz/ogre_helpers/shape.h"
#include "rviz/properties/float_property.h"
#include "rviz/properties/string_property.h"
#include "rviz/viewport_mouse_event.h"
#include "rviz/render_panel.h"
#include "rviz/selection/selection_manager.h"
#include "rviz/visualization_manager.h"

#include "ortho_view_controller_custom.h"

namespace rviz
{

OrthoViewControllerCustom::OrthoViewControllerCustom()
  : dragging_( false ),
    panel_( NULL )
{
  scale_property_ = new FloatProperty( "Scale", 5, "How much to scale up the size of things in the scene.", this );
  angle_property_ = new FloatProperty( "Angle", 0, "Angle around the Z axis to rotate.", this );
  x_property_ = new FloatProperty( "X", 0, "X component of camera position.", this );
  y_property_ = new FloatProperty( "Y", 0, "Y component of camera position.", this );
  view_plane_property_ = new StringProperty( "View Plane", "XY", "View plane for the camera.", this );
}

OrthoViewControllerCustom::~OrthoViewControllerCustom()
{
}

void OrthoViewControllerCustom::initialize( DisplayContext* context, rviz::RenderPanel* panel )
{
    //ROS_INFO("INITIALIZE");
    bool initialized = false;
    if(context != NULL)
        context_ = context;
    else
        initialized = true;

    panel_ = panel;

    if(!camera_)
        camera_ = panel_->getCamera();

    setValue( formatClassId( getClassId() ));
    setReadOnly( true );

    // Do subclass initialization.
    if(!initialized)
        onInitialize();

    /*cursor_ = getDefaultCursor();

    standard_cursors_[Default] = getDefaultCursor();
    standard_cursors_[Rotate2D] = makeIconCursor( "package://rviz/icons/rotate.svg" );
    standard_cursors_[Rotate3D] = makeIconCursor( "package://rviz/icons/rotate_cam.svg" );
    standard_cursors_[MoveXY] = makeIconCursor( "package://rviz/icons/move2d.svg" );
    standard_cursors_[MoveZ] = makeIconCursor( "package://rviz/icons/move_z.svg" );
    standard_cursors_[Zoom] = makeIconCursor( "package://rviz/icons/zoom.svg" );
    standard_cursors_[Crosshair] = makeIconCursor( "package://rviz/icons/crosshair.svg" );

    updateNearClipDistance();*/
}

void OrthoViewControllerCustom::onInitialize()
{
  //ROS_INFO("ONINITIALIZE");
  FramePositionTrackingViewController::onInitialize();

  camera_->setProjectionType( Ogre::PT_ORTHOGRAPHIC );
  camera_->setFixedYawAxis( false );
}

void OrthoViewControllerCustom::reset()
{
  scale_property_->setFloat( 5 );
  angle_property_->setFloat( 0 );
  x_property_->setFloat( 0 );
  y_property_->setFloat( 0 );
}

void OrthoViewControllerCustom::handleMouseEvent(ViewportMouseEvent& event)
{
//  if ( event.shift() )
//  {
    setStatus( "<b>Left-Click:</b> Move X/Y." );
//  }
//  else
//  {
//    setStatus( "<b>Left-Click:</b> Rotate.  <b>Middle-Click:</b> Move X/Y.  <b>Right-Click:</b>: Zoom.  <b>Shift</b>: More options." );
//  }

  bool moved = false;

  int32_t diff_x = 0;
  int32_t diff_y = 0;

  if( event.type == QEvent::MouseButtonPress )
  {
    dragging_ = true;
  }
  else if( event.type == QEvent::MouseButtonRelease )
  {
    dragging_ = false;
  }
  else if( dragging_ && event.type == QEvent::MouseMove )
  {
    diff_x = event.x - event.last_x;
    diff_y = event.y - event.last_y;
    moved = true;
  }

//  if( event.left() && !event.shift() )
//  {
//    setCursor( Rotate2D );
//    angle_property_->add( diff_x * 0.005 );
//    orientCamera();
//  }
//  else if( event.middle() || ( event.shift() && event.left() ))
//  {
//    setCursor( MoveXY );
//    float scale = scale_property_->getFloat();
//    move( -diff_x / scale, diff_y / scale );
//  }
//  else if( event.right() )
//  {
//    setCursor( Zoom );
//    scale_property_->multiply( 1.0 - diff_y * 0.01 );
//  }
//  else
//  {
//    setCursor( event.shift() ? MoveXY : Rotate2D );
//  }
  // can only do XY panning
  setCursor( MoveXY );

  if( event.middle() )
  {
      float scale = scale_property_->getFloat();
      move( -diff_x / scale, diff_y / scale );
  }

  if ( event.wheel_delta != 0 )
  {
    int diff = event.wheel_delta;
    scale_property_->multiply( 1.0 - (-diff) * 0.001 );

    moved = true;
  }

  if (moved)
  {
    context_->queueRender();
    emitConfigChanged();
  }
}

void OrthoViewControllerCustom::orientCamera()
{
  //camera_->setOrientation( Ogre::Quaternion( Ogre::Radian( angle_property_->getFloat() ), Ogre::Vector3::UNIT_Z ));
  //camera_->setOrientation( Ogre::Quaternion( Ogre::Radian( 90 ), Ogre::Vector3::UNIT_X ));
}

void OrthoViewControllerCustom::mimic( ViewController* source_view )
{
  FramePositionTrackingViewController::mimic( source_view );

  if( OrthoViewControllerCustom* source_ortho = qobject_cast<OrthoViewControllerCustom*>( source_view ))
  {
    scale_property_->setFloat( source_ortho->scale_property_->getFloat() );
    angle_property_->setFloat( source_ortho->angle_property_->getFloat() );
    x_property_->setFloat( source_ortho->x_property_->getFloat() );
    y_property_->setFloat( source_ortho->y_property_->getFloat() );
  }
  else
  {
    Ogre::Camera* source_camera = source_view->getCamera();
    setPosition( source_camera->getPosition() );
  }
}

void OrthoViewControllerCustom::update(float dt, float ros_dt)
{
  FramePositionTrackingViewController::update( dt, ros_dt );
  updateCamera();
  //std::cout << "custom camera" << std::endl;
}

void OrthoViewControllerCustom::lookAt( const Ogre::Vector3& point )
{
  setPosition( point - target_scene_node_->getPosition() );
}

void OrthoViewControllerCustom::onTargetFrameChanged(const Ogre::Vector3& old_reference_position, const Ogre::Quaternion& old_reference_orientation)
{
  move( old_reference_position.x - reference_position_.x,
        old_reference_position.y - reference_position_.y );
}

void OrthoViewControllerCustom::updateCamera()
{
  //orientCamera();

  if(!panel_)
    return;
  //ROS_INFO("UPDATE CAMERA");

  float width = panel_->getViewport()->getActualWidth();
  float height = panel_->getViewport()->getActualHeight();

  float scale = scale_property_->getFloat();
  Ogre::Matrix4 proj;
  buildScaledOrthoMatrix( proj, -width / scale / 2, width / scale / 2, -height / scale / 2, height / scale / 2,
                          camera_->getNearClipDistance(), camera_->getFarClipDistance() );
  camera_->setCustomProjectionMatrix(true, proj);

  // For Z, we use half of the far-clip distance set in
  // selection_context.cpp, so that the shader program which computes
  // depth can see equal distances above and below the Z=0 plane.
  if(view_plane_property_->getString() == "XY")
  {
    camera_->setPosition( x_property_->getFloat(), y_property_->getFloat(), 500 );
  	camera_->lookAt(x_property_->getFloat(), y_property_->getFloat(), 0);
  }
  else if(view_plane_property_->getString() == "XZ")
  {
    camera_->setFixedYawAxis( true, Ogre::Vector3::UNIT_Z );
    camera_->setPosition( x_property_->getFloat(), -500, y_property_->getFloat());
	camera_->lookAt(x_property_->getFloat(), 0, y_property_->getFloat());
  }
  else if(view_plane_property_->getString() == "YZ")
  {
    camera_->setFixedYawAxis( true, Ogre::Vector3::UNIT_Z );
    camera_->setPosition( 500, x_property_->getFloat(), y_property_->getFloat());
	camera_->lookAt(0, x_property_->getFloat(), y_property_->getFloat());
  }

  //ROS_INFO("  %x (%f, %f)  (%f, %f)", camera_->getViewport(), width, height, width / scale, height / scale);
  context_->getSelectionManager()->setOrthoConfig(camera_->getViewport(), width / scale, height / scale);
}

void OrthoViewControllerCustom::setPosition( const Ogre::Vector3& pos_rel_target )
{
  x_property_->setFloat( pos_rel_target.x );
  y_property_->setFloat( pos_rel_target.y );
}

void OrthoViewControllerCustom::move( float dx, float dy )
{
  float angle = angle_property_->getFloat();
  x_property_->add( dx*cos(angle)-dy*sin(angle) );
  y_property_->add( dx*sin(angle)+dy*cos(angle) );
}

} // end namespace rviz


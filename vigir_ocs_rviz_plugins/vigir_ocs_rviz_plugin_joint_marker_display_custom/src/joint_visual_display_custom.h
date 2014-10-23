#ifndef JOINT_VISUAL_DISPLAY_CUSTOM_H
#define JOINT_VISUAL_DISPLAY_CUSTOM_H

#include <OgreVector3.h>
#include <OgreSceneNode.h>
#include <OgreSceneManager.h>

#include <rviz/ogre_helpers/arrow.h>
#include <rviz/ogre_helpers/billboard_line.h>

#include <ros/ros.h>

#include <urdf/model.h>
#include <sensor_msgs/JointState.h>

namespace Ogre
{
    class Vector3;
    class Quaternion;
}

namespace urdf
{
    class Model;
}

namespace rviz
{
    class Arrow;
    class BillboardLine;
}

namespace rviz
{


// Each instance of EffortVisualCustom represents the visualization of a single
// sensor_msgs::Effort message.  Currently it just shows an arrow with
// the direction and magnitude of the acceleration vector, but could
// easily be expanded to include more of the message data.
class JointVisualCustom
{
public:
    // Constructor.  Creates the visual stuff and puts it into the
    // scene, but in an unconfigured state.
    JointVisualCustom( Ogre::SceneManager* scene_manager, Ogre::SceneNode* parent_node, boost::shared_ptr<urdf::Model> urdf_model );

    // Destructor.  Removes the visual stuff from the scene.
    virtual ~JointVisualCustom();

    //set arrow direction for specific joints
    void setArrowDirection(std::string jointName,int direction);

    // Configure the visual to show the data in the message.
    void setMessage( const sensor_msgs::JointStateConstPtr& msg );

    // Set the pose of the coordinate frame the message refers to.
    // These could be done inside setMessage(), but that would require
    // calls to FrameManager and error handling inside setMessage(),
    // which doesn't seem as clean.  This way EffortVisualCustom is only
    // responsible for visualization.
    void setFramePosition( const Ogre::Vector3& position );
    void setFrameOrientation( const Ogre::Quaternion& orientation );

    // set the pose of coordinates frame the each joint refers to.
    void setFramePosition( const std::string joint_name, const Ogre::Vector3& position );
    void setFrameOrientation( const std::string joint_name, const Ogre::Quaternion& orientation );

    //void setFrameEnabled( const std::string joint_name, const bool e );

    //set individual colors/alpha based on joint name to target specific joints
    void setJointColor( float r, float g, float b, std::string joint);
    void setJointAlpha(float a, std::string joint);

    void setWidth( float w );
    void setScale( float s );

    //only to be called from joint marker display custom, should be friended?
    void update( float wall_dt, float ros_dt );

    void setGhost(bool ghost);



private:
    void setRenderOrder(Ogre::SceneNode* sceneNode);

    //stores directions for all arrows, can be 1 or -1 to indicate direction, can be modified externally
    std::map<std::string,int> arrow_directions_;

    // The object implementing the effort circle           
    std::map<std::string, rviz::BillboardLine*> effort_circle_;
    std::map<std::string, rviz::Arrow*> effort_arrow_;
    std::map<std::string, bool> effort_enabled_;

    // A SceneNode whose pose is set to match the coordinate frame of
    // the Effort message header.
    Ogre::SceneNode* frame_node_;

    // The SceneManager, kept here only so the destructor can ask it to
    // destroy the ``frame_node_``.
    Ogre::SceneManager* scene_manager_;

    Ogre::ColourValue* color_;
    bool is_ghost_;
    float alpha_;

    std::map<std::string, Ogre::Vector3> position_;
    std::map<std::string, Ogre::Quaternion> orientation_;

    //store last msg for update function
    sensor_msgs::JointState::ConstPtr joint_msg_;

    //used to determine circular size of markers
    float marker_scale_;
    float width_, scale_;

    //determines the rate at which arrows move around circles
    float arrow_step_interval_;
    float arrow_timer_;

    std::map<std::string,int> current_arrow_point_;

    // The object for urdf model
    boost::shared_ptr<urdf::Model> urdf_model_;
};

} // end namespace rviz

#endif // JOINT_VISUAL_DISPLAY_CUSTOM_H


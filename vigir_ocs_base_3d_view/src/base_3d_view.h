/*
 * Base3DView class definition.
 *
 * Author: Felipe Bacim.
 *
 * Based on librviz_tutorials and the .
 *
 * Latest changes (12/08/2012):
 * - created class
 */

#ifndef BASE_3D_VIEW_H
#define BASE_3D_VIEW_H

#include <QWidget>
#include <QMouseEvent>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QLineEdit>
#include <QBasicTimer>
#include <QThread>
#include <QMenu>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTreeWidget>

#include <OGRE/OgreVector3.h>
#include <OGRE/OgreRay.h>
#include <OGRE/OgreSceneManager.h>
#include <OgreSubEntity.h>
#include <OgreHighLevelGpuProgramManager.h>

#include <ros/ros.h>

#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Point.h>
#include <std_msgs/Bool.h>
#include <std_msgs/String.h>
#include <nav_msgs/OccupancyGrid.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/JointState.h>
#include <mouse_event_handler.h>

#include <moveit_msgs/RobotState.h>
#include <moveit_msgs/DisplayRobotState.h>
//#include <moveit/robot_model_loader/robot_model_loader.h>
//#include <moveit/robot_model/robot_model.h>
//#include <moveit/robot_state/robot_state.h>
//#include <moveit/robot_state/conversions.h>

#include <flor_interactive_marker_server_custom/interactive_marker_server_custom.h>
#include <flor_ocs_msgs/OCSGhostControl.h>
#include <flor_ocs_msgs/OCSInteractiveMarkerAdd.h>
#include <flor_ocs_msgs/OCSInteractiveMarkerUpdate.h>
#include <flor_ocs_msgs/OCSKeyEvent.h>
#include <flor_ocs_msgs/OCSHotkeyRelay.h>
#include <flor_ocs_msgs/OCSObjectSelection.h>
#include "flor_ocs_msgs/OCSCameraTransform.h"
#include "flor_ocs_msgs/OCSControlMode.h"
#include <flor_perception_msgs/RaycastRequest.h>
#include <flor_perception_msgs/PointCloudTypeRegionRequest.h>
#include <flor_control_msgs/FlorControlModeCommand.h>
#include <flor_control_msgs/FlorControlMode.h>
#include <flor_ocs_msgs/OCSJoints.h>

#include "robot_custom.h"
#include "robot_link_custom.h"

#include <string>
#include <boost/bind.hpp>
#include <vector>
#include <map>
#define IM_MODE_OFFSET 3
#include <stdlib.h>

#include "robot_state_manager.h"

struct contextMenuItem
{
    QString name;
    //callback function of this item, sometimes null for parent items
    boost::function<void()> function;
    struct contextMenuItem * parent;
    //menu associated with this item, for children to add to menu
    QMenu* menu;
    //can only have action or menu. never both
    QAction* action;
    //tells whether to make an action or a menu object
    bool hasChildren;
};

namespace rviz
{
class Display;
class Tool;
class RenderPanel;
class RenderPanelCustom;
class VisualizationManager;
class ViewController;
class FrameManager;
class OrbitViewController;
class FPSViewController;
}

//class MoveItOcsModel;

namespace vigir_ocs
{

// Class "Main3DView" implements the RobotModel class with joint manipulation that can be added to any QT application.
class Base3DView: public QWidget
{
    Q_OBJECT
public:

    /**
      * @param copy_from Another instance of Base3DView containing the main rviz instance
      * @param base_frame Defines the frame in which this window will be rendered
      * @param widget_name The name of this window
      * @param parent Parent widget of this window
      * Class "Main3DView" implements the RobotModel class with joint manipulation that can be added to any QT application.
      */
    Base3DView( Base3DView* copy_from = NULL, std::string base_frame = "/pelvis", std::string widget_name = "", QWidget *parent = 0 );
    virtual ~Base3DView();

    /**
      *Helper function for the context menu: creates a sub menu
      */
    static contextMenuItem * makeContextParent(QString name,std::vector<contextMenuItem * > &contextMenuElements);
    /**
      * Helper function for the context menu: creates a selectable item
      */
    static contextMenuItem * makeContextChild(QString name,boost::function<void()> function,contextMenuItem * parent,std::vector<contextMenuItem * > &contextMenuElements);   

    /**
      * Helper function for context menu: stores template context menu structure
      */
    void setTemplateTree(QTreeWidget * root);

    /**
      * Helper function for context menu: adds an item to context menu
      */
    void addToContextVector(contextMenuItem* item);

    /**
      * ROS Callback: Receives occupancy map from onboard based on the slice of octomap
      */
    void processNewMap(const nav_msgs::OccupancyGrid::ConstPtr& pose);

    /**
      * ROS Callback: Processes a new selection pose from the ui (done with ctrl click)
      */
    void processNewSelection( const geometry_msgs::Point::ConstPtr& pose );

    /**
      * ROS Callback: Sets selection pose based on point cloud received from onboard
      */
    void processPointCloud( const sensor_msgs::PointCloud2::ConstPtr& pc );



    /**
      * ROS Callback: receives left arm end effector position from moveit
      */
    void processLeftArmEndEffector( const geometry_msgs::PoseStamped::ConstPtr& pose );
    /**
      * ROS Callback: receives right armend effector position from moveit
      */
    void processRightArmEndEffector( const geometry_msgs::PoseStamped::ConstPtr& pose );
    /**
      * ROS Callback: receives pelvis end effector position from moveit
      */
    void processPelvisEndEffector( const geometry_msgs::PoseStamped::ConstPtr &pose );

    /**
      * ROS Callback: receives left hand position to show grasps
      */
    void processLeftGhostHandPose( const geometry_msgs::PoseStamped::ConstPtr& pose );
    /**
      * ROS Callback: receives right hand position to show grasps
      */
    void processRightGhostHandPose( const geometry_msgs::PoseStamped::ConstPtr& pose );

    /**
      * ROS Callback: receives configuration message for ghost robot
      */
    void processGhostControlState( const flor_ocs_msgs::OCSGhostControl::ConstPtr& msg );

    /**
      * ROS Callback: receives joint states from the robot
      * calculates joint effort and position limits proceeds to call updateJointIcons()
      * TODO Create helper function for calculating joint efforts
      */
    void processJointStates( const sensor_msgs::JointState::ConstPtr& states );

    /**
      * ROS Callback: receives joint states from the ghost robot
      * calculates joint effort and position limits proceeds to call updateJointIcons()
      * TODO Create helper function for calculating joint efforts
      */
    void processGhostJointStates(const sensor_msgs::JointState::ConstPtr& states);

    /**
      * ROS Callback: receives request to reset ghost robot pose to robot pose
      */
    void processPelvisResetRequest( const std_msgs::Bool::ConstPtr& msg );

    /**
      * ROS Callback: trigger to send footstep planner request based on ghost
      */
    void processSendPelvisToFootstepRequest( const std_msgs::Bool::ConstPtr& msg );

    /**
      * ROS Callback: receives the current control mode from onboard
      */
    void processControlMode( const flor_control_msgs::FlorControlMode::ConstPtr& msg );

    /**
      * ROS Callback: receives configuration message for ghost robot
      */
    void processSendCartesian( const std_msgs::Bool::ConstPtr& msg );

    /**
      * ROS Callback: receives a new pose for the ghost robot
      */
    void processGhostPelvisPose(const geometry_msgs::PoseStamped::ConstPtr& msg);

    /**
      * ROS Callback: receives a new selected object and enables interactive marker if possible
      */
    void processObjectSelection(const flor_ocs_msgs::OCSObjectSelection::ConstPtr& msg);

    /**
      * ROS Callback: receives hotkey from secondary OCS
      */
    virtual void processHotkeyRelayMessage(const flor_ocs_msgs::OCSHotkeyRelay::ConstPtr& msg);
    /**
      * ROS Callback: receives new goal pose for footstep planner
      */
    virtual void processGoalPose( const geometry_msgs::PoseStamped::ConstPtr& pose, int type );
    /**
      * ROS Callback: receives new key event from global hotkey process
      */
    virtual void processNewKeyEvent(const flor_ocs_msgs::OCSKeyEvent::ConstPtr &key_event);

    /**
      * ROS Callback: receives interactive marker pose updates
      */
    void onMarkerFeedback( const flor_ocs_msgs::OCSInteractiveMarkerUpdate& msg );//std::string topic_name, geometry_msgs::PoseStamped pose);

    // functions needed for shared contexts
    rviz::VisualizationManager* getVisualizationManager() { return manager_; }
    rviz::Display* getSelection3DDisplay() { return selection_3d_display_; }
    MouseEventHandler* getMouseEventHander() { return mouse_event_handler_; }

    /**
      * Changes the OGRE render mask for this window which determines which object will be rendered
      */
    void updateRenderMask( bool );

    // returns name of the class that was instanced
    std::string getWidgetName() { return widget_name_; }

    /**
      * Utility method for comparing two poses with position and orientation thresholds
      */
    static bool checkPoseMatch(const geometry_msgs::Pose& p1, const geometry_msgs::Pose& p2, float scalar_error_threshold = 0.0f, float angle_error_threshold = 0.0f);

public Q_SLOTS:
    // displays
    // Enables/disables visibility of rviz displays
    void robotModelToggled( bool );
    void graspModelToggled( bool );
    void templatesToggled( bool );
    void requestedPointCloudToggled( bool );
    void lidarPointCloudToggled( bool );
    void stereoPointCloudToggled( bool );
    void laserScanToggled( bool );
    void ft_sensorToggled( bool );
    void markerArrayToggled( bool );
    void gridMapToggled( bool );
    void footstepPlanningToggled( bool );
    void simulationRobotToggled( bool );
    // tools
    // enables/disables use of rviz tools
    void cameraToggled( bool );
    void selectToggled( bool );
    void select3DToggled( bool );
    void markerRobotToggled( bool );
    void markerTemplateToggled( bool );
    void robotJointMarkerToggled(bool selected);
    void robotOcclusionToggled(bool selected);
    virtual void defineWalkPosePressed();
    virtual void defineStepPosePressed();


    // Sets position of new selection marker
    void newSelection( Ogre::Vector3 );
    //adds new template with name
    void insertTemplate( QString );
    //
    void templatePathChanged( QString );
    void insertWaypoint();

    virtual void createContextMenu( bool, int, int );
    virtual void processContextMenu( int x, int y );
    // sends back the context
    void setContext( int, std::string );

    // get the last selection ray
    void setSelectionRay( Ogre::Ray );

    void publishPointCloudWorldRequest();

    void publishMarkers();

    void resetView();

    void clearPointCloudRaycastRequests();
    void clearPointCloudStereoRequests();
    void clearPointCloudRegionRequests();
    void clearMapRequests();

    void sendCartesianLeft();
    void sendCartesianRight();
    void sendCircularLeft();
    void sendCircularRight();

    void selectOnDoubleClick(int,int);

    virtual bool eventFilter( QObject * o, QEvent * e );
    void setRenderOrder();

Q_SIGNALS:
    void setRenderPanel( rviz::RenderPanel* );
    void resetSelection();
    void setMarkerScale( float );
    // send position of the mouse when clicked to create context menu
    void queryContext( int, int );
    void setMarkerPosition( float, float, float );
    void enableTemplateMarker( int, bool );
    void enableTemplateMarkers( bool );
    void setFrustum( const float &, const float &, const float&, const float& );
    void finishedContextMenuSetup( int x, int y );
    void sendPositionText(QString s);
    void updateMainViewItems();
    void emergencyStop();


protected:
    bool shift_pressed_;
    int interactive_marker_mode_;

    /**
      * Helper Function: deselects all objects in the current view
      **/
    void deselectAll();

    void updateJointIcons(const std::string& name, const geometry_msgs::Pose& pose,double effortPercent, double boundPercent);
    int findObjectContext(std::string obj_type);
    void selectLeftArm();
    void selectRightArm();
    void setTemplateGraspLock(int arm);
    void addTemplatesToContext();
    void contextInsertTemplate(QString name);
    void addBase3DContextElements();
    void processContextMenuVector();
    void addToContextMenuFromVector();
    void snapHandGhost();
    std::vector<contextMenuItem*> contextMenuItems;
    virtual void timerEvent(QTimerEvent *event);
    void transform(const std::string& target_frame, geometry_msgs::PoseStamped& pose);
    void transform(Ogre::Vector3& position, Ogre::Quaternion& orientation, const char* from_frame, const char* to_frame);

    void removeTemplate(int id);

    void publishGhostPoses();
    virtual rviz::ViewController* getCurrentViewController();

    void publishHandPose(std::string hand, const geometry_msgs::PoseStamped& end_effector_transform);
    void publishHandJointStates(std::string hand);
    void publishCameraTransform();
    int calcWristTarget(const geometry_msgs::PoseStamped& end_effector_pose, tf::Transform hand_T_palm, geometry_msgs::PoseStamped& final_pose);
    void sendCartesianTarget(bool right_hand, std::vector<geometry_msgs::Pose> waypoints);
    void sendCircularTarget(bool right_hand);

    void selectTemplate(int id);
    void selectContextMenu();

    void modeCB(const flor_ocs_msgs::OCSControlMode::ConstPtr& msg);

    void updateHandColors();

    void updateGhostJointsCb(const std::string& name, const geometry_msgs::Pose& pose);


    Ogre::Camera* getCamera();

    rviz::VisualizationManager* manager_;
    rviz::RenderPanel* render_panel_;

    //fps and orbit view controllers
    rviz::OrbitViewController* orbit_view_controller_;
    rviz::FPSViewController* fps_view_controller_;

    rviz::Display* robot_model_;    
    std::vector<rviz::Display*> im_ghost_robot_;
    //std::vector<InteractiveMarkerServerCustom*> im_ghost_robot_server_;
    rviz::Display* interactive_marker_template_;
    rviz::Display* octomap_;
    rviz::Display* grid_;
    rviz::Display* laser_scan_;
    rviz::Display* region_point_cloud_viewer_;
    rviz::Display* stereo_point_cloud_viewer_;
    rviz::Display* selection_3d_display_;
    rviz::Display* template_display_;
    rviz::Display* waypoints_display_;
    rviz::Display* achieved_waypoints_display_;
    rviz::Display* octomap_roi_;
    rviz::Display* raycast_point_cloud_viewer_;
    std::map<std::string,rviz::Display*> frustum_viewer_list_;

    // new displays for walking
    rviz::Display* footsteps_array_;
    std::vector<rviz::Display*> ground_map_;
    rviz::Display* goal_pose_walk_;
    rviz::Display* goal_pose_step_;
    rviz::Display* planner_start_;
    rviz::Display* planned_path_;
    rviz::Display* footsteps_path_body_array_;

    rviz::Display* left_ft_sensor_;
    rviz::Display* right_ft_sensor_;

    rviz::Display* left_grasp_hand_model_;
    rviz::Display* right_grasp_hand_model_;

    rviz::Display* left_hand_model_;
    robot_model_loader::RobotModelLoaderPtr left_hand_model_loader_;
    robot_model::RobotModelPtr left_hand_robot_model_;
    robot_state::RobotStatePtr left_hand_robot_state_;
    moveit_msgs::DisplayRobotState left_display_state_msg_;
    ros::Publisher left_hand_robot_state_vis_pub_;
    // Used to make setting virtual joint positions (-> hand pose) easier
    sensor_msgs::JointState left_hand_virtual_link_joint_states_;

    rviz::Display* right_hand_model_;
    robot_model_loader::RobotModelLoaderPtr right_hand_model_loader_;
    robot_model::RobotModelPtr right_hand_robot_model_;
    robot_state::RobotStatePtr right_hand_robot_state_;
    moveit_msgs::DisplayRobotState right_display_state_msg_;
    ros::Publisher right_hand_robot_state_vis_pub_;
    // Used to make setting virtual joint positions (-> hand pose) easier
    sensor_msgs::JointState right_hand_virtual_link_joint_states_;

    // for simulation
    rviz::Display* ghost_robot_model_;

    std::map<std::string,rviz::Display*> display_list_;

    rviz::Tool* interactive_markers_tool_;
    //rviz::Tool* selection_tool_;
    rviz::Tool* move_camera_tool_;
    rviz::Tool* set_walk_goal_tool_;
    rviz::Tool* set_step_goal_tool_;

    rviz::Display* left_hand_bounding_box_;
    rviz::Display* right_hand_bounding_box_;
    rviz::Display* pelvis_hand_bounding_box_;

    Ogre::Vector3 selection_position_;

    ros::NodeHandle nh_;

    ros::Publisher template_add_pub_;
    ros::Publisher waypoint_add_pub_;

    ros::Publisher octomap_roi_pub_;

    ros::Publisher global_selection_pos_pub_;
    ros::Subscriber global_selection_pos_sub_;

    ros::Subscriber ground_map_sub_;
    ros::Subscriber point_cloud_result_sub_;

    ros::Publisher pointcloud_request_world_pub_;

    std::vector<ros::Subscriber> end_effector_sub_;
    ros::Publisher end_effector_pub_;
    ros::Publisher ghost_root_pose_pub_;
    std::map<std::string,geometry_msgs::PoseStamped> end_effector_pose_list_;

    ros::Subscriber ghost_control_state_sub_;

    ros::Publisher ghost_joint_state_pub_;
    ros::Subscriber joint_states_sub_;
    ros::Subscriber reset_pelvis_sub_;
    ros::Subscriber send_pelvis_sub_;
    ros::Publisher send_footstep_goal_step_pub_;
    ros::Publisher send_footstep_goal_walk_pub_;

    ros::Subscriber set_walk_goal_sub_;
    ros::Subscriber set_step_goal_sub_;

    ros::Publisher interactive_marker_add_pub_;
    ros::Publisher interactive_marker_update_pub_;
    ros::Subscriber interactive_marker_feedback_sub_;
    ros::Publisher interactive_marker_remove_pub_;
    ros::Publisher interactive_marker_server_mode_pub_;
    ros::Subscriber interactive_marker_server_mode_sub_;

    ros::Subscriber ghost_hand_left_sub_;
    ros::Subscriber ghost_hand_right_sub_;

    ros::Publisher flor_mode_command_pub_;
    ros::Subscriber flor_mode_sub_;
    ros::Subscriber robot_joint_state_sub_;

    ros::Publisher select_object_pub_;
    ros::Subscriber select_object_sub_;

    ros::Publisher camera_transform_pub_;
    ros::Subscriber camera_transform_sub_;

    std::vector<unsigned char> ghost_planning_group_;
    std::vector<unsigned char> ghost_pose_source_;
    std::vector<unsigned char> ghost_world_lock_;
    unsigned char moveit_collision_avoidance_;
    unsigned char ghost_lock_pelvis_;
    bool update_markers_;
    bool snap_ghost_to_robot_;
    bool snap_left_hand_to_ghost_;
    bool snap_right_hand_to_ghost_;
    bool left_marker_moveit_loopback_;
    bool right_marker_moveit_loopback_;
    bool position_only_ik_;

    vigir_ocs::MouseEventHandler* mouse_event_handler_;

    std::string base_frame_;
    std::string widget_name_;

    bool selected_;
    QString selected_template_path_;

    int active_context_;

    int last_footstep_plan_type_;

    Ogre::Ray last_selection_ray_;

    int marker_published_;
    int stored_maps_;// THIS VALUE DETERMINES HOW MANY WE STORE

    bool moving_pelvis_;
    bool moving_l_arm_;
    bool moving_r_arm_;

    bool visualize_grid_map_;
    QWidget* position_widget_;
    QLineEdit* position_label_;

    QPushButton* reset_view_button_;
    QPushButton* stop_button_;

    tf::Transform l_hand_T_palm_;
    tf::Transform r_hand_T_palm_;

    tf::Transform l_hand_T_marker_;
    tf::Transform r_hand_T_marker_;

    QBasicTimer timer;

    int view_id_;

    std::string l_hand_type, r_hand_type;

    QMenu context_menu_;
    QAction* context_menu_selected_item_;

    int initializing_context_menu_;
    std::string active_context_name_;

    ros::Publisher template_remove_pub_;

    int flor_atlas_current_mode_;

    std::vector<int> keys_pressed_list_;

    ros::Subscriber key_event_sub_;
    ros::Subscriber hotkey_relay_sub_;

    bool is_primary_view_;

    geometry_msgs::Pose last_l_arm_moveit_pose_;
    geometry_msgs::Pose last_r_arm_moveit_pose_;
    geometry_msgs::Pose last_l_arm_marker_pose_;
    geometry_msgs::Pose last_r_arm_marker_pose_;
    bool update_l_arm_color_;
    bool update_r_arm_color_;

    ros::Publisher l_arm_marker_pose_pub_;
    ros::Publisher r_arm_marker_pose_pub_;
    ros::Publisher pelvis_marker_pose_pub_;

    std::vector<rviz::Display*> cartesian_marker_list_;
    rviz::Display* circular_marker_;

    std::vector<geometry_msgs::Pose> cartesian_waypoint_list_;
    geometry_msgs::Pose circular_center_;

    ros::Publisher cartesian_plan_request_pub_;
    ros::Publisher circular_plan_request_pub_;

    QWidget* cartesian_config_widget_;
    QCheckBox* cartesian_use_collision_;
    QCheckBox* cartesian_keep_orientation_;

    QWidget* circular_config_widget_;
    QCheckBox* circular_use_collision_;
    QCheckBox* circular_keep_orientation_;
    QDoubleSpinBox* circular_angle_;

    ros::Subscriber send_cartesian_sub_;
    ros::Subscriber send_ghost_pelvis_pose_sub_;
    ros::Subscriber ghost_joint_state_sub_;
    geometry_msgs::Pose ghost_root_pose_;

    void insertTemplateContextMenu();
    void removeTemplateContextMenu();
    void executeFootstepPlanContextMenu();
    void createCartesianContextMenu();
    void removeCartesianContextMenu();
    void createCircularContextMenu();
    void removeCircularContextMenu();

    contextMenuItem * insertTemplateMenu;
    contextMenuItem * removeTemplateMenu;
    contextMenuItem * selectMenu;
    contextMenuItem * footstepPlanMenuWalk;
    contextMenuItem * footstepPlanMenuWalkManipulation;
    contextMenuItem * cartesianMotionMenu;
    contextMenuItem * createCartesianMarkerMenu;
    contextMenuItem * removeCartesianMarkerMenu;
    contextMenuItem * circularMotionMenu;
    contextMenuItem * createCircularMarkerMenu;
    contextMenuItem * removeCircularMarkerMenu;
    contextMenuItem * lockLeftMenu;
    contextMenuItem * lockRightMenu;
    contextMenuItem * unlockArmsMenu;
    contextMenuItem * snapHandMenu;
    contextMenuItem * leftArmMenu;
    contextMenuItem * rightArmMenu;

    QTreeWidget * templateRoot;

    flor_ocs_msgs::OCSJoints jointStates;   

    std::map<std::string,rviz::Display*> jointDisplayMap;

    std::vector<int> ghostJointStates;


    typedef std::map< std::string, rviz::RobotLinkCustom* > M_NameToLink;
    typedef std::map<Ogre::SubEntity*, Ogre::MaterialPtr> M_SubEntityToMaterial;
    void setRobotOccludedRender();    
    void disableRobotOccludedRender();
    void setSceneNodeRenderGroup(Ogre::SceneNode* sceneNode, int queueOffset);

    bool disableJointMarkers;
    sensor_msgs::JointState::ConstPtr latest_ghost_joint_state_;

};
}
#endif // BASE_3D_VIEW_H

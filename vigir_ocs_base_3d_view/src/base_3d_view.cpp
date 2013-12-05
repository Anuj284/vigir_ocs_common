/*
 * Base3DView class implementation.
 *
 * Author: Felipe Bacim.
 *
 * Based on librviz_tutorials.
 *
 * Latest changes (12/08/2012):
 * - added support for joint manipulation?
 */

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPoint>
#include <QWidgetAction>
#include <QSlider>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/common/common.h>
#include <pcl/common/transforms.h>

#include <pcl_conversions/pcl_conversions.h>

#include "rviz/visualization_manager.h"
#include "rviz/visualization_frame.h"
//#include "rviz/render_panel.h"
#include <render_panel_custom.h>
#include "rviz/display.h"
#include "rviz/frame_manager.h"
#include "rviz/tool_manager.h"
#include "rviz/view_manager.h"
#include "rviz/default_plugin/view_controllers/fixed_orientation_ortho_view_controller.h"
#include "rviz/default_plugin/view_controllers/orbit_view_controller.h"
#include "rviz/properties/parse_color.h"
#include <template_display_custom.h>
#include "map_display_custom.h"
#include "base_3d_view.h"

#include "flor_ocs_msgs/OCSTemplateAdd.h"
#include "flor_ocs_msgs/OCSTemplateRemove.h"
#include "flor_ocs_msgs/OCSWaypointAdd.h"
#include "flor_perception_msgs/EnvironmentRegionRequest.h"
#include "flor_planning_msgs/TargetConfigIkRequest.h"

namespace vigir_ocs
{

// Constructor for Base3DView.  This does most of the work of the class.
Base3DView::Base3DView( Base3DView* copy_from, std::string base_frame, QWidget* parent )
    : QWidget( parent )
    , base_frame_(base_frame)
    , selected_(false)
    , update_markers_(false)
    , snap_ghost_to_robot_(true)
    , marker_published_(0)
    , stored_maps_(30) // determines how many maps will be stored
    , moving_pelvis_(false)
    , moving_l_arm_(false)
    , moving_r_arm_(false)
    , left_marker_moveit_loopback_(true)
    , right_marker_moveit_loopback_(true)
    , visualize_grid_map_(true)
    , initializing_context_menu_(0)
{
    // Construct and lay out render panel.
    render_panel_ = new rviz::RenderPanelCustom();
    ((rviz::RenderPanelCustom*)render_panel_)->setEventFilters(rviz::RenderPanelCustom::MOUSE_PRESS_EVENT,false,Qt::NoModifier,Qt::RightButton);
    ((rviz::RenderPanelCustom*)render_panel_)->setEventFilters(rviz::RenderPanelCustom::MOUSE_RELEASE_EVENT,false,Qt::NoModifier,Qt::RightButton);
    ((rviz::RenderPanelCustom*)render_panel_)->setEventFilters(rviz::RenderPanelCustom::MOUSE_MOVE_EVENT,false,Qt::NoModifier,Qt::RightButton);

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->setMargin(0);
    main_layout->addWidget( render_panel_ );

    // Set the top-level layout for this MyViz widget.
    setLayout( main_layout );

    // if there's
    if(copy_from != NULL)
    {
        manager_ = copy_from->getVisualizationManager();
        render_panel_->initialize( manager_->getSceneManager(), manager_ );
        view_id_ = manager_->addRenderPanel( render_panel_ );

        selection_3d_display_ = copy_from->getSelection3DDisplay();
        mouse_event_handler_ = copy_from->getMouseEventHander();

        // Connect the 3D selection tool to
        QObject::connect(this, SIGNAL(queryContext(int,int)), selection_3d_display_, SLOT(queryContext(int,int)));
        QObject::connect(selection_3d_display_, SIGNAL(setContext(int,std::string)), this, SLOT(setContext(int,std::string)));

        // connect the 3d selection tool to its display
        QObject::connect(selection_3d_display_, SIGNAL(newSelection(Ogre::Vector3)), this, SLOT(newSelection(Ogre::Vector3)));
        QObject::connect(selection_3d_display_, SIGNAL(setSelectionRay(Ogre::Ray)), this, SLOT(setSelectionRay(Ogre::Ray)));
        QObject::connect(this, SIGNAL(resetSelection()), selection_3d_display_, SLOT(resetSelection()));
        QObject::connect(this, SIGNAL(setMarkerScale(float)), selection_3d_display_, SLOT(setMarkerScale(float)));
        QObject::connect(this, SIGNAL(setMarkerPosition(float,float,float)), selection_3d_display_, SLOT(setMarkerPosition(float,float,float)));

        // handles mouse events without rviz::tool
        QObject::connect(render_panel_, SIGNAL(signalMousePressEvent(QMouseEvent*)), mouse_event_handler_, SLOT(mousePressEvent(QMouseEvent*)));
        QObject::connect(render_panel_, SIGNAL(signalMouseReleaseEvent(QMouseEvent*)), mouse_event_handler_, SLOT(mouseReleaseEvent(QMouseEvent*)));
        QObject::connect(mouse_event_handler_, SIGNAL(mouseLeftButtonCtrl(bool,int,int)), selection_3d_display_, SLOT(raycastRequest(bool,int,int)));//SLOT(createMarker(bool,int,int))); // RAYCAST -> need createMarkerOnboard that sends raycast query
        QObject::connect(mouse_event_handler_, SIGNAL(mouseLeftButtonShift(bool,int,int)), selection_3d_display_, SLOT(raycastRequestROI(bool,int,int)));//SLOT(createROISelection(bool,int,int)));
        QObject::connect(mouse_event_handler_, SIGNAL(mouseRightButton(bool,int,int)), this, SLOT(createContextMenu(bool,int,int)));
    }
    else
    {
        // Next we initialize the main RViz classes.
        //
        // The VisualizationManager is the container for Display objects,
        // holds the main Ogre scene, holds the ViewController, etc.  It is
        // very central and we will probably need one in every usage of
        // librviz.
        manager_ = new rviz::VisualizationManager( render_panel_ );
        render_panel_->initialize( manager_->getSceneManager(), manager_ );
        view_id_ = manager_->addRenderPanel( render_panel_ );

        Ogre::SceneNode* lightSceneNode = NULL;
        Ogre::Light* light = manager_->getSceneManager()->createLight();

        // I can set some attributes of the light.
        // The basic light type can be :
        //		pointlight (like a candle?)
        //		spotlight (kind of 'conic' light)
        //		directional light (like the sun in an outdoor scene).
        // Directional light is like parallel rays coming from 1 direction.
        light->setType(Ogre::Light::LT_DIRECTIONAL);

        // Here I choose the color of the light.
        // The diffuse color is the main color of the light.
        // The specular color is its color when reflected on an imperfect surface.
        // For example, when my bald head skin reflect the sun, it makes a bright round of specular color.
        //
        // The final color of an object also depends on its material.
        // Color values vary between 0.0(minimum) to 1.0 (maximum).
        light->setDiffuseColour(0.25f, 0.25f, 0.25f); // this will be a red light
        light->setSpecularColour(1.0f, 1.0f, 1.0f);// color of 'reflected' light

        lightSceneNode = manager_->getSceneManager()->getRootSceneNode()->createChildSceneNode();
        lightSceneNode->attachObject(light);

        // Set topic that will be used as 0,0,0 -> reference for all the other transforms
        // IMPORTANT: WITHOUT THIS, ALL THE DIFFERENT PARTS OF THE ROBOT MODEL WILL BE DISPLAYED AT 0,0,0
        manager_->setFixedFrame(base_frame_.c_str());

        manager_->initialize();
        manager_->startUpdate();

        // First remove all existin tools
        manager_->getToolManager()->removeAll();
        // Add support for interactive markers
        interactive_markers_tool_ = manager_->getToolManager()->addTool( "rviz/InteractionToolCustom" );
        // Add support for selection
        //selection_tool_ = manager_->getToolManager()->addTool( "rviz/Select" );
        // Add support for camera movement
        move_camera_tool_ = manager_->getToolManager()->addTool( "rviz/MoveCamera" );
        // Add support for goal specification/vector navigation
        set_walk_goal_tool_ = manager_->getToolManager()->addTool( "rviz/SetGoal" );
        set_walk_goal_tool_->getPropertyContainer()->subProp( "Topic" )->setValue( "/goal_pose_walk" );
        set_step_goal_tool_ = manager_->getToolManager()->addTool( "rviz/SetGoal" );
        set_step_goal_tool_->getPropertyContainer()->subProp( "Topic" )->setValue( "/goal_pose_step" );

        // Create a LaserScan display.
        laser_scan_ = manager_->createDisplay( "rviz/LaserScan", "Laser Scan", false );
        ROS_ASSERT( laser_scan_ != NULL );
        laser_scan_->subProp( "Topic" )->setValue( "/laser/scan" );
        laser_scan_->subProp( "Style" )->setValue( "Points" );
        laser_scan_->subProp( "Size (Pixels)" )->setValue( 3 );
        //laser_scan_->subProp( "Size (m)" )->setValue( 0.1 );
        laser_scan_->subProp( "Decay Time" )->setValue( 5 );
        laser_scan_->subProp( "Color Transformer" )->setValue( "AxisColor" );
        laser_scan_->subProp( "Axis" )->setValue( "Z" );
        laser_scan_->subProp( "Selectable" )->setValue( false );

        // Create a MarkerArray display.
        octomap_ = manager_->createDisplay( "rviz/OctomapDisplayCustom", "Octomap", false );
        ROS_ASSERT( octomap_ != NULL );
        octomap_->subProp( "Marker Topic" )->setValue( "/worldmodel_main/occupied_cells_vis_array" );

        // Create a point cloud display.
        stereo_point_cloud_viewer_ = manager_->createDisplay( "rviz/PointCloud2", "Point Cloud", false );
        ROS_ASSERT( stereo_point_cloud_viewer_ != NULL );
        stereo_point_cloud_viewer_->subProp( "Style" )->setValue( "Points" );
        stereo_point_cloud_viewer_->subProp( "Topic" )->setValue( "/multisense_sl/camera/points2" );
        stereo_point_cloud_viewer_->subProp( "Size (Pixels)" )->setValue( 3 );
        stereo_point_cloud_viewer_->subProp( "Selectable" )->setValue( false );

        lidar_point_cloud_viewer_ = manager_->createDisplay( "rviz/PointCloud2", "Point Cloud", false );
        ROS_ASSERT( lidar_point_cloud_viewer_ != NULL );
        lidar_point_cloud_viewer_->subProp( "Style" )->setValue( "Points" );
        lidar_point_cloud_viewer_->subProp( "Topic" )->setValue( "/worldmodel_main/pointcloud_vis" );
        lidar_point_cloud_viewer_->subProp( "Size (Pixels)" )->setValue( 2 );
        lidar_point_cloud_viewer_->subProp( "Color Transformer" )->setValue( "AxisColor" );
        lidar_point_cloud_viewer_->subProp( "Axis" )->setValue( "Z" );
        lidar_point_cloud_viewer_->subProp( "Selectable" )->setValue( false );

        // point cloud request
        point_cloud_request_viewer_ = manager_->createDisplay( "rviz/PointCloud2", "Point Cloud", true );
        point_cloud_request_viewer_->subProp( "Style" )->setValue( "Points" );
        point_cloud_request_viewer_->subProp( "Topic" )->setValue( "/flor/worldmodel/ocs/dist_query_pointcloud_result" );
        point_cloud_request_viewer_->subProp( "Size (Pixels)" )->setValue( 3 );
        point_cloud_request_viewer_->subProp( "Color Transformer" )->setValue( "AxisColor" );
        point_cloud_request_viewer_->subProp( "Decay Time" )->setValue( 9999999 );
        point_cloud_request_viewer_->subProp( "Selectable" )->setValue( false );

        // Create a template display to display all templates listed by the template nodelet
        template_display_ = manager_->createDisplay( "rviz/TemplateDisplayCustom", "Template Display", true );
        ((rviz::TemplateDisplayCustom*)template_display_)->setVisualizationManager(manager_);

        // Create a display for 3D selection
        selection_3d_display_ = manager_->createDisplay( "rviz/Selection3DDisplayCustom", "3D Selection Display", true );

        // Create a display for waypoints
        waypoints_display_ = manager_->createDisplay( "rviz/PathDisplayCustom", "Path Display", true );
        waypoints_display_->subProp( "Topic" )->setValue( "/waypoint/list" );

        // Create another display for waypoints, this time the ones that have already been achieved
        achieved_waypoints_display_ = manager_->createDisplay( "rviz/PathDisplayCustom", "Path Display", true );
        achieved_waypoints_display_->subProp( "Topic" )->setValue( "/waypoint/achieved_list" );
        achieved_waypoints_display_->subProp( "Color" )->setValue( QColor( 150, 150, 255 ) );


        // create a publisher to add templates
        template_add_pub_   = nh_.advertise<flor_ocs_msgs::OCSTemplateAdd>( "/template/add", 1, false );

        // create a publisher to add waypoints
        waypoint_add_pub_   = nh_.advertise<flor_ocs_msgs::OCSWaypointAdd>( "/waypoint/add", 1, false );

        selection_position_ = Ogre::Vector3(0,0,0);

        // Make the move camera tool the currently selected one
        manager_->getToolManager()->setCurrentTool( move_camera_tool_ );

        // Footstep array
        footsteps_array_ = manager_->createDisplay( "rviz/MarkerArray", "Footsteps array", true );
        footsteps_array_->subProp( "Marker Topic" )->setValue( "/flor/walk_monitor/footsteps_array" );

        goal_pose_walk_ = manager_->createDisplay( "rviz/Pose", "Goal pose", true );
        goal_pose_walk_->subProp( "Topic" )->setValue( "/goal_pose_walk" );
        goal_pose_walk_->subProp( "Shape" )->setValue( "Axes" );

        goal_pose_step_ = manager_->createDisplay( "rviz/Pose", "Goal pose", true );
        goal_pose_step_->subProp( "Topic" )->setValue( "/goal_pose_step" );
        goal_pose_step_->subProp( "Shape" )->setValue( "Axes" );

        planner_start_ = manager_->createDisplay( "rviz/Pose", "Start pose", true );
        planner_start_->subProp( "Topic" )->setValue( "/ros_footstep_planner/start" );
        planner_start_->subProp( "Shape" )->setValue( "Axes" );

        planned_path_ = manager_->createDisplay( "rviz/Path", "Planned path", true );
        planned_path_->subProp( "Topic" )->setValue( "/flor/walk_monitor/path" );

        // create the grasp hands displays
        left_grasp_hand_model_ = manager_->createDisplay( "moveit_rviz_plugin/RobotState", "Robot left hand model", true );
        left_grasp_hand_model_->subProp( "Robot Description" )->setValue( "left_hand_robot_description" );
        left_grasp_hand_model_->subProp( "Robot State Topic" )->setValue( "/flor/ghost/template_left_hand" );
        left_grasp_hand_model_->subProp( "Robot Root Link" )->setValue( "base" );
        left_grasp_hand_model_->subProp( "Robot Alpha" )->setValue( 0.5f );

        right_grasp_hand_model_ = manager_->createDisplay( "moveit_rviz_plugin/RobotState", "Robot right hand model", true );
        right_grasp_hand_model_->subProp( "Robot Description" )->setValue( "right_hand_robot_description" );
        right_grasp_hand_model_->subProp( "Robot State Topic" )->setValue( "/flor/ghost/template_right_hand" );
        right_grasp_hand_model_->subProp( "Robot Root Link" )->setValue( "base" );
        right_grasp_hand_model_->subProp( "Robot Alpha" )->setValue( 0.5f );

        // create the hands displays
        left_hand_model_ = manager_->createDisplay( "moveit_rviz_plugin/RobotState", "Robot left hand model", true );
        left_hand_model_->subProp( "Robot Description" )->setValue( "left_hand_robot_description" );
        left_hand_model_->subProp( "Robot State Topic" )->setValue( "/flor/ghost/marker_left_hand" );
        left_hand_model_->subProp( "Robot Root Link" )->setValue( "base" );
        left_hand_model_->subProp( "Robot Alpha" )->setValue( 0.5f );

        left_hand_model_loader_.reset(new robot_model_loader::RobotModelLoader("left_hand_robot_description"));
        left_hand_robot_model_ = left_hand_model_loader_->getModel();
        left_hand_robot_state_.reset(new robot_state::RobotState(left_hand_robot_model_));
        left_hand_robot_state_vis_pub_ = nh_.advertise<moveit_msgs::DisplayRobotState>("/flor/ghost/marker_left_hand",1, true);

        {
            // change color of the ghost template hands
            const std::vector<std::string>& link_names = left_hand_robot_model_->getLinkModelNames();

            for (size_t i = 0; i < link_names.size(); ++i)
            {
                moveit_msgs::ObjectColor tmp;
                tmp.id = link_names[i];
                tmp.color.a = 0.5f;
                tmp.color.r = 0.2f;
                tmp.color.g = 0.8f;
                tmp.color.b = 0.2f;
                left_display_state_msg_.highlight_links.push_back(tmp);
            }
        }

        // this is for publishing the hand position in world coordinates for moveit
        left_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_x");
        left_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_y");
        left_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_z");
        left_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_x");
        left_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_y");
        left_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_z");
        left_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_w");
        left_hand_virtual_link_joint_states_.position.resize(7);

        right_hand_model_ = manager_->createDisplay( "moveit_rviz_plugin/RobotState", "Robot right hand model", true );
        right_hand_model_->subProp( "Robot Description" )->setValue( "right_hand_robot_description" );
        right_hand_model_->subProp( "Robot State Topic" )->setValue( "/flor/ghost/marker_right_hand" );
        right_hand_model_->subProp( "Robot Root Link" )->setValue( "base" );
        right_hand_model_->subProp( "Robot Alpha" )->setValue( 0.5f );

        right_hand_model_loader_.reset(new robot_model_loader::RobotModelLoader("right_hand_robot_description"));
        right_hand_robot_model_ = right_hand_model_loader_->getModel();
        right_hand_robot_state_.reset(new robot_state::RobotState(right_hand_robot_model_));
        right_hand_robot_state_vis_pub_ = nh_.advertise<moveit_msgs::DisplayRobotState>("/flor/ghost/marker_right_hand",1, true);

        {
            // change color of the ghost template hands
            const std::vector<std::string>& link_names = right_hand_robot_model_->getLinkModelNames();

            for (size_t i = 0; i < link_names.size(); ++i)
            {
                moveit_msgs::ObjectColor tmp;
                tmp.id = link_names[i];
                tmp.color.a = 0.5f;
                tmp.color.r = 0.2f;
                tmp.color.g = 0.8f;
                tmp.color.b = 0.2f;
                right_display_state_msg_.highlight_links.push_back(tmp);
            }
        }

        // this is for publishing the hand position in world coordinates for moveit
        right_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_x");
        right_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_y");
        right_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_z");
        right_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_x");
        right_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_y");
        right_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_z");
        right_hand_virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_w");
        right_hand_virtual_link_joint_states_.position.resize(7);

        // create the simulation robot display
        ghost_robot_model_ = manager_->createDisplay( "moveit_rviz_plugin/RobotState", "Robot simulation model", false );
        ghost_robot_model_->subProp( "Robot State Topic" )->setValue( "/flor/ghost/robot_state_vis" );
        ghost_robot_model_->subProp( "Robot Alpha" )->setValue( 0.0f );
        ghost_robot_model_->setEnabled(false);

        // Add custom interactive markers to control ghost robot
        rviz::Display* im_left_arm = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker - left arm", false );
        im_left_arm->subProp( "Update Topic" )->setValue( "/l_arm_pose_marker/pose_marker/update" );
        im_left_arm->subProp( "Show Axes" )->setValue( true );
        im_left_arm->subProp( "Show Visual Aids" )->setValue( true );
        im_ghost_robot_.push_back(im_left_arm);
        rviz::Display* im_right_arm = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker - right arm", false );
        im_right_arm->subProp( "Update Topic" )->setValue( "/r_arm_pose_marker/pose_marker/update" );
        im_right_arm->subProp( "Show Axes" )->setValue( true );
        im_right_arm->subProp( "Show Visual Aids" )->setValue( true );
        im_ghost_robot_.push_back(im_right_arm);
        rviz::Display* im_pelvis = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker - pelvis", false );
        im_pelvis->subProp( "Update Topic" )->setValue( "/pelvis_pose_marker/pose_marker/update" );
        im_pelvis->subProp( "Show Axes" )->setValue( true );
        im_pelvis->subProp( "Show Visual Aids" )->setValue( true );
        im_ghost_robot_.push_back(im_pelvis);

        //geometry_msgs::Point point;
        //static InteractiveMarkerServerCustom* marker_server_left_arm = new InteractiveMarkerServerCustom("Ghost Left Arm", "/l_arm_pose_marker", manager_->getFixedFrame().toStdString(), 0.4, point);
        //im_ghost_robot_server_.push_back(marker_server_left_arm);
        //im_ghost_robot_server_[im_ghost_robot_server_.size()-1]->onFeedback = boost::bind(&Base3DView::onMarkerFeedback, this, _1, _2);
        //static InteractiveMarkerServerCustom* marker_server_right_arm = new InteractiveMarkerServerCustom("Ghost Right Arm", "/r_arm_pose_marker", manager_->getFixedFrame().toStdString(), 0.4, point);
        //im_ghost_robot_server_.push_back(marker_server_right_arm);
        //im_ghost_robot_server_[im_ghost_robot_server_.size()-1]->onFeedback = boost::bind(&Base3DView::onMarkerFeedback, this, _1, _2);
        //static InteractiveMarkerServerCustom* marker_server_pelvis = new InteractiveMarkerServerCustom("Ghost Pelvis", "/pelvis_pose_marker", manager_->getFixedFrame().toStdString(), 0.4, point);
        //.push_back(marker_server_pelvis);
        //im_ghost_robot_server_[im_ghost_robot_server_.size()-1]->onFeedback = boost::bind(&Base3DView::onMarkerFeedback, this, _1, _2);

        interactive_marker_add_pub_ = nh_.advertise<flor_ocs_msgs::OCSInteractiveMarkerAdd>( "/flor/ocs/interactive_marker_server/add", 1, true );
        interactive_marker_update_pub_ = nh_.advertise<flor_ocs_msgs::OCSInteractiveMarkerUpdate>( "/flor/ocs/interactive_marker_server/update", 1, false );
        interactive_marker_feedback_sub_ = nh_.subscribe<flor_ocs_msgs::OCSInteractiveMarkerUpdate>( "/flor/ocs/interactive_marker_server/feedback", 5, &Base3DView::onMarkerFeedback, this );;

        // subscribe to the pose topics
        end_effector_sub_.push_back(nh_.subscribe<geometry_msgs::PoseStamped>( "/flor/ghost/pose/left_hand", 5, &Base3DView::processLeftArmEndEffector, this ));
        end_effector_sub_.push_back(nh_.subscribe<geometry_msgs::PoseStamped>( "/flor/ghost/pose/right_hand", 5, &Base3DView::processRightArmEndEffector, this ));

        end_effector_pub_ = nh_.advertise<flor_planning_msgs::TargetConfigIkRequest>( "/flor/ghost/set_appendage_poses", 1, false );

        ghost_root_pose_pub_ = nh_.advertise<geometry_msgs::PoseStamped>( "/flor/ghost/set_root_pose", 1, false );
        ghost_joint_state_pub_ = nh_.advertise<sensor_msgs::JointState>( "/flor/ghost/set_joint_states", 1, false );

        // subscribe to the ghost hands pose
        ghost_hand_left_sub_ = nh_.subscribe<geometry_msgs::PoseStamped>( "/ghost_left_hand_pose", 5, &Base3DView::processLeftGhostHandPose, this );
        ghost_hand_right_sub_ = nh_.subscribe<geometry_msgs::PoseStamped>( "/ghost_right_hand_pose", 5, &Base3DView::processRightGhostHandPose, this );

        // initialize ghost control config
        ghost_planning_group_.push_back(0);
        ghost_planning_group_.push_back(1);
        ghost_planning_group_.push_back(1);
        ghost_pose_source_.push_back(0);
        ghost_pose_source_.push_back(0);
        ghost_pose_source_.push_back(0);
        ghost_world_lock_.push_back(0);
        ghost_world_lock_.push_back(0);
        ghost_world_lock_.push_back(0);
        moveit_collision_avoidance_ = 0;
        ghost_lock_pelvis_ = 1;

        // ghost state
        ghost_control_state_sub_ = nh_.subscribe<flor_ocs_msgs::OCSGhostControl>( "/flor/ocs/ghost_ui_state", 5, &Base3DView::processGhostControlState, this );
        reset_pelvis_sub_ = nh_.subscribe<std_msgs::Bool>( "/flor/ocs/reset_pelvis", 5, &Base3DView::processPelvisResetRequest, this );
        send_pelvis_sub_ = nh_.subscribe<std_msgs::Bool>( "/flor/ocs/send_pelvis_to_footstep", 5, &Base3DView::processSendPelvisToFootstepRequest, this );
        send_footstep_goal_pub_ = nh_.advertise<geometry_msgs::PoseStamped>( "/goalpose", 1, false );

        // Create a RobotModel display.
        robot_model_ = manager_->createDisplay( "rviz/RobotDisplayCustom", "Robot model", true );
        robot_model_->subProp( "Color" )->setValue( QColor( 200, 200, 200 ) );
        robot_model_->subProp( "Alpha" )->setValue( 1.0f );

        // ground map middle man
        //ground_map_sub_ = n_.subscribe<nav_msgs::OccupancyGrid>( "/flor/worldmodel/grid_map_near_robot", 5, &Base3DView::processNewMap, this );
        ground_map_sub_ = nh_.subscribe<nav_msgs::OccupancyGrid>( "/flor/worldmodel/ocs/gridmap_result", 5, &Base3DView::processNewMap, this );

        // point cloud request/selection publisher
        point_cloud_result_sub_ =  nh_.subscribe<sensor_msgs::PointCloud2>( "/flor/worldmodel/ocs/dist_query_pointcloud_result", 5, &Base3DView::processPointCloud, this );
        global_selection_pos_pub_ = nh_.advertise<geometry_msgs::Point>( "/new_point_cloud_request", 1, false );
        global_selection_pos_sub_ = nh_.subscribe<geometry_msgs::Point>( "/new_point_cloud_request", 5, &Base3DView::processNewSelection, this );

        joint_states_sub_ = nh_.subscribe<sensor_msgs::JointState>( "atlas/joint_states", 2, &Base3DView::processJointStates, this );

        // advertise pointcloud request
        pointcloud_request_world_pub_ = nh_.advertise<flor_perception_msgs::RaycastRequest>( "/flor/worldmodel/ocs/dist_query_pointcloud_request_world", 1, false );

        // frustum
        frustum_viewer_list_["head_left"] = manager_->createDisplay( "rviz/FrustumDisplayCustom", "Frustum - Left Eye", true );
        //}

        // and advertise the template remove option
        template_remove_pub_ = nh_.advertise<flor_ocs_msgs::OCSTemplateRemove>( "/template/remove", 1, false );

        // Connect to the template markers
        QObject::connect(this, SIGNAL(enableTemplateMarkers(bool)), template_display_, SLOT(enableTemplateMarkers(bool)));

        // set frustum
        QObject::connect(this, SIGNAL(setFrustum(const float&,const float&,const float&,const float&)), frustum_viewer_list_["head_left"], SLOT(setFrustum(const float&,const float&,const float&,const float&)));

    }

    position_widget_ = new QWidget(this);
    position_widget_->setStyleSheet("background-color: rgb(0, 0, 0);color: rgb(108, 108, 108);border-color: rgb(0, 0, 0);");
    position_widget_->setMaximumHeight(18);
    position_label_ = new QLineEditSmall("",position_widget_);
    position_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    position_label_->setReadOnly(true);
    position_label_->setStyleSheet("background-color: rgb(0, 0, 0);font: 8pt \"MS Shell Dlg 2\";color: rgb(108, 108, 108);border-color: rgb(0, 0, 0);");
    position_label_->setFrame(false);

    reset_view_button_ = new QPushButton("Reset View", this);
    reset_view_button_->setStyleSheet("font: 8pt \"MS Shell Dlg 2\";color: rgb(108, 108, 108);border-color: rgb(0, 0, 0);");
    reset_view_button_->setMaximumSize(68,18);
    reset_view_button_->adjustSize();
    QObject::connect(reset_view_button_, SIGNAL(clicked()), this, SLOT(resetView()));

    QHBoxLayout* position_layout = new QHBoxLayout();
    position_layout->setSpacing(0);
    position_layout->setMargin(0);
    position_layout->addWidget(reset_view_button_);
    position_layout->addWidget(position_label_);
    position_widget_->setLayout(position_layout);
    main_layout->setMargin(0);
    main_layout->setSpacing(0);
    main_layout->addWidget(position_widget_);

    XmlRpc::XmlRpcValue   hand_T_palm;

    nh_.getParam("/l_hand_tf/hand_T_palm", hand_T_palm);
    l_hand_T_palm_.setOrigin(tf::Vector3(static_cast<double>(hand_T_palm[0]),static_cast<double>(hand_T_palm[1]),static_cast<double>(hand_T_palm[2])));
    l_hand_T_palm_.setRotation(tf::Quaternion(static_cast<double>(hand_T_palm[3]),static_cast<double>(hand_T_palm[4]),static_cast<double>(hand_T_palm[5]),static_cast<double>(hand_T_palm[6])));

    nh_.getParam("/r_hand_tf/hand_T_palm", hand_T_palm);
    r_hand_T_palm_.setOrigin(tf::Vector3(static_cast<double>(hand_T_palm[0]),static_cast<double>(hand_T_palm[1]),static_cast<double>(hand_T_palm[2])));
    r_hand_T_palm_.setRotation(tf::Quaternion(static_cast<double>(hand_T_palm[3]),static_cast<double>(hand_T_palm[4]),static_cast<double>(hand_T_palm[5]),static_cast<double>(hand_T_palm[6])));

    nh_.getParam("/l_hand_type", l_hand_type);
    nh_.getParam("/r_hand_type", r_hand_type);

    // set background color to rviz default
    render_panel_->getViewport()->setBackgroundColour(rviz::qtToOgre(QColor(48,48,48)));

    // this is only used to make sure we close window if ros::shutdown has already been called
    timer.start(33, this);
}

// Destructor.
Base3DView::~Base3DView()
{
    delete manager_;
}

void Base3DView::timerEvent(QTimerEvent *event)
{
    // check if ros is still running; if not, just kill the application
    if(!ros::ok())
        qApp->quit();

    // make sure the selection point is visible
    //position_widget_->setGeometry(0,
    //                              this->geometry().bottomLeft().y()-18,
    //                              this->geometry().bottomRight().x()-this->geometry().bottomLeft().x()+2,
    //                              20);



    //std::bitset<32> x(render_panel_->getViewport()->getVisibilityMask());
    //std::cout << x << std::endl;

    //render_panel_->getRenderWindow()->update(true);

    //float lastFPS, avgFPS, bestFPS, worstFPS;
    //Ogre::RenderTarget::FrameStats stats = render_panel_->getRenderWindow()->getStatistics();
    //std::cout << "View (" << view_id_ << "): " << stats.lastFPS << ", " << stats.avgFPS << ", " << stats.bestFrameTime << ", " << stats.worstFrameTime << ", " << stats.triangleCount << std::endl;

    // no need to spin as rviz is already doing that for us.
    //ros::spinOnce();
}

void Base3DView::updateRenderMask( bool mask )
{
    manager_->updateRenderMask( view_id_, mask );
}

void Base3DView::robotModelToggled( bool selected )
{
    robot_model_->setEnabled( selected );
}

void Base3DView::graspModelToggled( bool selected )
{
    left_grasp_hand_model_->setEnabled( selected );
    right_grasp_hand_model_->setEnabled( selected );
}

void Base3DView::templatesToggled( bool selected )
{
    template_display_->setEnabled( selected );
}

void Base3DView::requestedPointCloudToggled( bool selected )
{
    // we can't enable/disable point cloud requests, since the existing ones are lost if we disable them
    point_cloud_request_viewer_->subProp("Alpha")->setValue(selected ? 1.0f : 0.0f);
}

void Base3DView::lidarPointCloudToggled( bool selected )
{
    lidar_point_cloud_viewer_->setEnabled( selected );
}

void Base3DView::stereoPointCloudToggled( bool selected )
{
    stereo_point_cloud_viewer_->setEnabled( selected );
}

void Base3DView::laserScanToggled( bool selected )
{
    laser_scan_->setEnabled( selected );
}

void Base3DView::markerArrayToggled( bool selected )
{
    // we can't enable/disable octomaps, since the existing ones are lost if we disable them
    //octomap_->subProp("Alpha")->setValue(selected ? 1.0f : 0.0f);
    octomap_->setEnabled(selected);
}

void Base3DView::gridMapToggled( bool selected )
{
    // we can't enable/disable gridmaps, since the existing ones are lost if we disable them
    visualize_grid_map_ = selected;
    if(!selected)
    {
        for(int i = 0; i < ground_map_.size(); i++)
        {
            ground_map_[i]->subProp("Alpha")->setValue(0.0f);
        }
    }
    else
    {
        float alpha = 1.0f, step = (0.95f/stored_maps_);
        unsigned short priority = stored_maps_-1;
        for(int i = ground_map_.size()-1; i >= 0; i--,alpha-=step)
        {
            //std::cout << i << " " << alpha << std::endl;
            ground_map_[i]->subProp("Alpha")->setValue(alpha);
            ((rviz::MapDisplayCustom*)ground_map_[i])->setPriority(priority--);
        }
    }
}

void Base3DView::footstepPlanningToggled( bool selected )
{
    goal_pose_walk_->setEnabled( selected );
    goal_pose_step_->setEnabled( selected );
    planner_start_->setEnabled( selected );
    planned_path_->setEnabled( selected );
    footsteps_array_->setEnabled( selected );
}

void Base3DView::simulationRobotToggled( bool selected )
{
    ghost_robot_model_->setEnabled( selected );
    if(selected)
    {
        update_markers_ = true;

        publishGhostPoses();
    }

    ghost_robot_model_->subProp( "Robot Alpha" )->setValue( 0.5f );
}

void Base3DView::cameraToggled( bool selected )
{
    if(selected)
    {
        manager_->getToolManager()->setCurrentTool( move_camera_tool_ );

        // enable robot IK markers
        for( int i = 0; i < im_ghost_robot_.size(); i++ )
        {
            im_ghost_robot_[i]->setEnabled( false );
        }

        // disable template marker
        Q_EMIT enableTemplateMarkers( false );
    }
}

void Base3DView::selectToggled( bool selected )
{
    //if(selected)
    //    manager_->getToolManager()->setCurrentTool( selection_tool_ );
}

void Base3DView::select3DToggled( bool selected )
{
    //if(selected)
    //    manager_->getToolManager()->setCurrentTool( selection_3d_tool_ );
}

void Base3DView::markerRobotToggled( bool selected )
{
    if(selected)
    {
        manager_->getToolManager()->setCurrentTool( interactive_markers_tool_ );

        // enable robot IK markers
        for( int i = 0; i < im_ghost_robot_.size(); i++ )
        {
            im_ghost_robot_[i]->setEnabled( true );
        }

        publishGhostPoses();

        // disable template marker
        Q_EMIT enableTemplateMarkers( false );
    }
}

void Base3DView::markerTemplateToggled( bool selected )
{
    if(selected)
    {
        manager_->getToolManager()->setCurrentTool( interactive_markers_tool_ );

        // disable robot IK markers
        for( int i = 0; i < im_ghost_robot_.size(); i++ )
        {
            im_ghost_robot_[i]->setEnabled( false );
        }
        // enable template markers
        Q_EMIT enableTemplateMarkers( true );
    }
}

void Base3DView::defineWalkPosePressed()
{
    //set_goal_tool_->getPropertyContainer()->subProp( "Topic" )->setValue( "/goal_pose_walk" );
    manager_->getToolManager()->setCurrentTool( set_walk_goal_tool_ );
}

void Base3DView::defineStepPosePressed()
{
    //set_goal_tool_->getPropertyContainer()->subProp( "Topic" )->setValue( "/goal_pose_step" );
    manager_->getToolManager()->setCurrentTool( set_step_goal_tool_ );
}

void Base3DView::processPointCloud( const sensor_msgs::PointCloud2::ConstPtr& pc )
{
    //std::cout << "point cloud received" << std::endl;
    pcl::PointCloud<pcl::PointXYZ> pclCloud;
    pcl::fromROSMsg(*pc, pclCloud);
    Eigen::Vector4f centroid;
    if(pcl::compute3DCentroid(pclCloud,centroid) != 0)
    {
        geometry_msgs::Point point;
        point.x = centroid[0];
        point.y = centroid[1];
        point.z = centroid[2];
        //std::cout << "centroid: " << point.x << ", " << point.y << ", " << point.z << std::endl;
        global_selection_pos_pub_.publish(point);
    }
}

void Base3DView::processNewSelection( const geometry_msgs::Point::ConstPtr& pose )
{
    Q_EMIT setMarkerPosition(pose->x,pose->y,pose->z);
}

void Base3DView::newSelection( Ogre::Vector3 position )
{
    QString new_label = QString::number(position.x,'f',2)+", "+QString::number(position.y,'f',2)+", "+QString::number(position.z,'f',2);
    position_label_->setText(new_label);

    selection_position_ = position;
    selected_ = true;
}

void Base3DView::templatePathChanged( QString path )
{
    selected_template_path_ = path;
}

void Base3DView::transform(Ogre::Vector3& position, Ogre::Quaternion& orientation, const char* from_frame, const char* to_frame)
{
    //std::cout << "POS bt: " << position.x << ", " << position.y << ", " << position.z << std::endl;
    // put all pose data into a tf stamped pose
    tf::Quaternion bt_orientation(orientation.x, orientation.y, orientation.z, orientation.w);
    tf::Vector3 bt_position(position.x, position.y, position.z);

    std::string frame(from_frame);
    tf::Stamped<tf::Pose> pose_in(tf::Transform(bt_orientation,bt_position), ros::Time(), frame);
    tf::Stamped<tf::Pose> pose_out;

    // convert pose into new frame
    try
    {
      manager_->getFrameManager()->getTFClient()->transformPose( to_frame, pose_in, pose_out );
    }
    catch(tf::TransformException& e)
    {
      ROS_DEBUG("Error transforming from frame '%s' to frame '%s': %s", from_frame, to_frame, e.what());
      return;
    }

    bt_position = pose_out.getOrigin();
    position = Ogre::Vector3(bt_position.x(), bt_position.y(), bt_position.z());
    //std::cout << "POS transform: " << position.x << ", " << position.y << ", " << position.z << std::endl;

    bt_orientation = pose_out.getRotation();
    orientation = Ogre::Quaternion( bt_orientation.w(), bt_orientation.x(), bt_orientation.y(), bt_orientation.z() );
    //std::cout << "QUAT transform: " << orientation.x << ", " << orientation.y << ", " << orientation.z << ", " << orientation.w << std::endl;
}

void Base3DView::transform(const std::string& target_frame, geometry_msgs::PoseStamped& pose)
{

    tf::Quaternion bt_orientation(pose.pose.orientation.x,pose.pose.orientation.y,pose.pose.orientation.z,pose.pose.orientation.w);
    tf::Vector3 bt_position(pose.pose.position.x,pose.pose.position.y,pose.pose.position.z);

    tf::Stamped<tf::Pose> pose_in(tf::Transform(bt_orientation,bt_position), ros::Time(), pose.header.frame_id);
    tf::Stamped<tf::Pose> pose_out;

    try
    {
        manager_->getFrameManager()->getTFClient()->transformPose( target_frame.c_str(), pose_in, pose_out );
    }
    catch(tf::TransformException& e)
    {
        ROS_DEBUG("Error transforming from frame '%s' to frame '%s': %s", pose.header.frame_id.c_str(), target_frame.c_str(), e.what());
        return;
    }

    bt_position = pose_out.getOrigin();
    bt_orientation = pose_out.getRotation();

    pose.pose.position.x = bt_position.x();
    pose.pose.position.y = bt_position.y();
    pose.pose.position.z = bt_position.z();
    pose.pose.orientation.x = bt_orientation.x();
    pose.pose.orientation.y = bt_orientation.y();
    pose.pose.orientation.z = bt_orientation.z();
    pose.pose.orientation.w = bt_orientation.w();

    pose.header.frame_id = target_frame;
}

void Base3DView::insertTemplate( QString path )
{
    //std::cout << "adding template" << std::endl;

    if(!selected_)
    {
        flor_ocs_msgs::OCSTemplateAdd cmd;
        geometry_msgs::PoseStamped pose;

        cmd.template_path = path.toStdString();

        pose.pose.position.x = 1;
        pose.pose.position.y = 0;
        pose.pose.position.z = .2;
        pose.pose.orientation.x = 0;
        pose.pose.orientation.y = 0;
        pose.pose.orientation.z = 0;
        pose.pose.orientation.w = 1;
        pose.header.frame_id = "/pelvis";
        transform("/world",pose);

        cmd.pose = pose;

        // publish complete list of templates and poses
        template_add_pub_.publish( cmd );
    }
    else
    {
        flor_ocs_msgs::OCSTemplateAdd cmd;
        geometry_msgs::PoseStamped pose;

        cmd.template_path = path.toStdString();

        pose.pose.position.x = selection_position_.x;
        pose.pose.position.y = selection_position_.y;
        pose.pose.position.z = selection_position_.z;
        pose.pose.orientation.x = 0;
        pose.pose.orientation.y = 0;
        pose.pose.orientation.z = 0;
        pose.pose.orientation.w = 1;

        pose.header.frame_id = "/world";

        cmd.pose = pose;

        // publish complete list of templates and poses
        template_add_pub_.publish( cmd );

        selected_ = false;

        Q_EMIT resetSelection();
    }
}

void Base3DView::insertWaypoint()
{
    if(selected_)
    {
        //std::cout << "adding waypoint" << std::endl;

        flor_ocs_msgs::OCSWaypointAdd cmd;
        geometry_msgs::PoseStamped pose;

        pose.pose.position.x = selection_position_.x;
        pose.pose.position.y = selection_position_.y;
        pose.pose.position.z = selection_position_.z;
        pose.pose.orientation.x = 0;
        pose.pose.orientation.y = 0;
        pose.pose.orientation.z = 0;
        pose.pose.orientation.w = 1;

        pose.header.frame_id = "/world";

        cmd.pose = pose;

        // publish complete list of templates and poses
        waypoint_add_pub_.publish( cmd );

        selected_ = false;
        Q_EMIT resetSelection();
    }
}

void Base3DView::createContextMenu(bool, int x, int y)
{
    initializing_context_menu_++;

    context_menu_selected_item_ = NULL;

    context_menu_.clear();
    context_menu_.setTitle("Base Menu");

    // first we need to query the 3D scene to retrieve the context
    Q_EMIT queryContext(x,y);
    // context is stored in the active_context_ variable
    std::cout << "Active context: " << active_context_ << std::endl;

    context_menu_.addAction("Insert Template");
    //if(selected_) context_menu_.addAction("Insert Waypoint");
    if(active_context_name_.find("template") != std::string::npos) context_menu_.addAction("Remove Template");

    if(initializing_context_menu_ == 1)
        processContextMenu(x, y);

    initializing_context_menu_--;
}

void Base3DView::processContextMenu(int x, int y)
{
    QPoint globalPos = this->mapToGlobal(QPoint(x,y));
    context_menu_selected_item_ = context_menu_.exec(globalPos);

    //std::cout << selectedItem << std::endl;
    if(context_menu_selected_item_ != NULL)
    {
        if(context_menu_selected_item_->text() == QString("Insert Template"))
        {
            if(!selected_template_path_.isEmpty())
                insertTemplate(selected_template_path_);
        }
        else if(context_menu_selected_item_->text() == QString("Remove Template"))
        {
            int start = active_context_name_.find(" ")+1;
            int end = active_context_name_.find(".");
            QString template_number(active_context_name_.substr(start, end-start).c_str());
            ROS_INFO("%d %d %s",start,end,template_number.toStdString().c_str());
            bool ok;
            int t = template_number.toInt(&ok);
            if(ok) removeTemplate(t);
        }
        //else if(context_menu_selected_item_->text() == QString("Insert Waypoint"))
        //{
        //    insertWaypoint();
        //}
    }
}

void Base3DView::removeTemplate(int id)
{
    flor_ocs_msgs::OCSTemplateRemove cmd;

    cmd.template_id = id;

    // publish template to be removed
    template_remove_pub_.publish( cmd );
}

void Base3DView::setContext(int context, std::string name)
{
    active_context_ = context;
    active_context_name_ = name;
    //std::cout << "Active context: " << active_context_ << std::endl;
}

void Base3DView::setSelectionRay( Ogre::Ray ray )
{
    last_selection_ray_ = ray;
}

void Base3DView::publishPointCloudWorldRequest()
{
    flor_perception_msgs::RaycastRequest request;

    request.origin.x = last_selection_ray_.getOrigin().x;
    request.origin.y = last_selection_ray_.getOrigin().y;
    request.origin.z = last_selection_ray_.getOrigin().z;

    request.direction.x = last_selection_ray_.getDirection().x;
    request.direction.y = last_selection_ray_.getDirection().y;
    request.direction.z = last_selection_ray_.getDirection().z;

    pointcloud_request_world_pub_.publish(request);
}

void Base3DView::processNewMap(const nav_msgs::OccupancyGrid::ConstPtr &map)
{
    static int counter = 0;
    std::stringstream map_topic;
    map_topic << "map_topic_" << counter++;
    rviz::Display* ground_map = manager_->createDisplay( "rviz/MapDisplayCustom", "Ground map", true );
    ((rviz::MapDisplayCustom*)ground_map)->incomingMap(map);
    ground_map_.push_back(ground_map);
    if(ground_map_.size() > stored_maps_)
    {
        rviz::Display* tmp = ground_map_[0];
        tmp->setEnabled(false);
        delete tmp;
        manager_->notifyConfigChanged();
        ground_map_.erase(ground_map_.begin());
    }
    gridMapToggled(visualize_grid_map_);
}

void Base3DView::processLeftArmEndEffector(const geometry_msgs::PoseStamped::ConstPtr &pose)
{
    if(left_marker_moveit_loopback_)
    {
        if(marker_published_ < 3)
            publishMarkers();

        publishHandPose("left",*pose);

        geometry_msgs::PoseStamped wrist_pose;
        calcWristTarget(*pose,l_hand_T_palm_,wrist_pose);

        flor_ocs_msgs::OCSInteractiveMarkerUpdate cmd;
        cmd.topic = "/l_arm_pose_marker";
        cmd.pose = wrist_pose;
        interactive_marker_update_pub_.publish(cmd);

        //ROS_ERROR("LEFT ARM POSE:");
        //ROS_ERROR("  position: %.2f %.2f %.2f",cmd.pose.pose.position.x,cmd.pose.pose.position.y,cmd.pose.pose.position.z);
        //ROS_ERROR("  orientation: %.2f %.2f %.2f %.2f",cmd.pose.pose.orientation.w,cmd.pose.pose.orientation.x,cmd.pose.pose.orientation.y,cmd.pose.pose.orientation.z);
        if(!moving_pelvis_ && ghost_pose_source_[0] == 0)
            end_effector_pose_list_[cmd.topic] = cmd.pose;
    }
}

void Base3DView::processRightArmEndEffector(const geometry_msgs::PoseStamped::ConstPtr &pose)
{
    if(right_marker_moveit_loopback_)
    {
        if(marker_published_ < 3)
            publishMarkers();

        publishHandPose("right",*pose);

        geometry_msgs::PoseStamped wrist_pose;
        calcWristTarget(*pose,r_hand_T_palm_,wrist_pose);

        flor_ocs_msgs::OCSInteractiveMarkerUpdate cmd;
        cmd.topic = "/r_arm_pose_marker";
        cmd.pose = wrist_pose;
        interactive_marker_update_pub_.publish(cmd);

        //ROS_ERROR("RIGHT ARM POSE:");
        //ROS_ERROR("  position: %.2f %.2f %.2f",cmd.pose.pose.position.x,cmd.pose.pose.position.y,cmd.pose.pose.position.z);
        //ROS_ERROR("  orientation: %.2f %.2f %.2f %.2f",cmd.pose.pose.orientation.w,cmd.pose.pose.orientation.x,cmd.pose.pose.orientation.y,cmd.pose.pose.orientation.z);
        if(!moving_pelvis_ && ghost_pose_source_[1] == 0)
            end_effector_pose_list_[cmd.topic] = cmd.pose;
    }
}

int staticTransform(geometry_msgs::Pose& palm_pose, tf::Transform hand_T_palm)
{
    tf::Transform o_T_hand;    //describes hand in object's frame
    tf::Transform o_T_palm;       //describes palm_from_graspit in object's frame

    o_T_palm.setRotation(tf::Quaternion(palm_pose.orientation.x,palm_pose.orientation.y,palm_pose.orientation.z,palm_pose.orientation.w));
    o_T_palm.setOrigin(tf::Vector3(palm_pose.position.x,palm_pose.position.y,palm_pose.position.z) );

    o_T_hand = o_T_palm * hand_T_palm.inverse();

    //ROS_INFO("hand_T_palm: p=(%f, %f, %f) q=(%f, %f, %f, %f)",
    //         hand_T_palm.getOrigin().getX(),hand_T_palm.getOrigin().getY(),hand_T_palm.getOrigin().getZ(),
    //         hand_T_palm.getRotation().getW(),hand_T_palm.getRotation().getX(),hand_T_palm.getRotation().getY(),hand_T_palm.getRotation().getZ());

    tf::Quaternion hand_quat;
    tf::Vector3    hand_vector;
    hand_quat   = o_T_hand.getRotation();
    hand_vector = o_T_hand.getOrigin();

    palm_pose.position.x = hand_vector.getX();
    palm_pose.position.y = hand_vector.getY();
    palm_pose.position.z = hand_vector.getZ();
    palm_pose.orientation.x = hand_quat.getX();
    palm_pose.orientation.y = hand_quat.getY();
    palm_pose.orientation.z = hand_quat.getZ();
    palm_pose.orientation.w = hand_quat.getW();

    return 0;
}

void Base3DView::publishHandPose(std::string hand, const geometry_msgs::PoseStamped& end_effector_transform)
{
    geometry_msgs::PoseStamped hand_transform; // the first hand transform is really where I want the palm to be, or identity in this case
    if(hand == "left")
    {
        calcWristTarget(end_effector_transform, l_hand_T_palm_, hand_transform);

        left_hand_virtual_link_joint_states_.position[0] = hand_transform.pose.position.x;
        left_hand_virtual_link_joint_states_.position[1] = hand_transform.pose.position.y;
        left_hand_virtual_link_joint_states_.position[2] = hand_transform.pose.position.z;
        left_hand_virtual_link_joint_states_.position[3] = hand_transform.pose.orientation.x;
        left_hand_virtual_link_joint_states_.position[4] = hand_transform.pose.orientation.y;
        left_hand_virtual_link_joint_states_.position[5] = hand_transform.pose.orientation.z;
        left_hand_virtual_link_joint_states_.position[6] = hand_transform.pose.orientation.w;

        moveit::core::jointStateToRobotState(left_hand_virtual_link_joint_states_, *left_hand_robot_state_);
    }
    else
    {
        calcWristTarget(end_effector_transform, r_hand_T_palm_, hand_transform);

        right_hand_virtual_link_joint_states_.position[0] = hand_transform.pose.position.x;
        right_hand_virtual_link_joint_states_.position[1] = hand_transform.pose.position.y;
        right_hand_virtual_link_joint_states_.position[2] = hand_transform.pose.position.z;
        right_hand_virtual_link_joint_states_.position[3] = hand_transform.pose.orientation.x;
        right_hand_virtual_link_joint_states_.position[4] = hand_transform.pose.orientation.y;
        right_hand_virtual_link_joint_states_.position[5] = hand_transform.pose.orientation.z;
        right_hand_virtual_link_joint_states_.position[6] = hand_transform.pose.orientation.w;

        moveit::core::jointStateToRobotState(right_hand_virtual_link_joint_states_, *right_hand_robot_state_);
    }

    publishHandJointStates(hand);
}

void Base3DView::publishHandJointStates(std::string hand)
{
    std::string hand_type;
    if(hand == "left")
        hand_type = l_hand_type;
    else
        hand_type = r_hand_type;

    sensor_msgs::JointState joint_states;

    joint_states.header.stamp = ros::Time::now();
    joint_states.header.frame_id = std::string("/")+hand+std::string("_hand_model/")+hand+"_palm";

    if(hand_type.find("irobot") != std::string::npos)
    {
        // must match the order used in the .grasp file
        joint_states.name.push_back(hand+"_f0_j1");
        joint_states.name.push_back(hand+"_f1_j1");
        joint_states.name.push_back(hand+"_f2_j1");
        joint_states.name.push_back(hand+"_f0_j0"); // .grasp finger position [4] -> IGNORE [3], use [4] for both
        joint_states.name.push_back(hand+"_f1_j0"); // .grasp finger position [4]
        joint_states.name.push_back(hand+"_f0_j2"); // 0 for now
        joint_states.name.push_back(hand+"_f1_j2"); // 0 for now
        joint_states.name.push_back(hand+"_f2_j2"); // 0 for now

    }
    else if(hand_type.find("sandia") != std::string::npos)
    {
        // must match those inside of the /sandia_hands/?_hand/joint_states/[right_/left_]+
        joint_states.name.push_back(hand+"_f0_j0");
        joint_states.name.push_back(hand+"_f0_j1");
        joint_states.name.push_back(hand+"_f0_j2");
        joint_states.name.push_back(hand+"_f1_j0");
        joint_states.name.push_back(hand+"_f1_j1");
        joint_states.name.push_back(hand+"_f1_j2");
        joint_states.name.push_back(hand+"_f2_j0");
        joint_states.name.push_back(hand+"_f2_j1");
        joint_states.name.push_back(hand+"_f2_j2");
        joint_states.name.push_back(hand+"_f3_j0");
        joint_states.name.push_back(hand+"_f3_j1");
        joint_states.name.push_back(hand+"_f3_j2");
    }

    joint_states.position.resize(joint_states.name.size());
    joint_states.effort.resize(joint_states.name.size());
    joint_states.velocity.resize(joint_states.name.size());

    for(unsigned int i = 0; i < joint_states.position.size(); ++i)
    {
        joint_states.effort[i] = 0;
        joint_states.velocity[i] = 0;
        joint_states.position[i] = 0;
    }

    if(hand == "left")
    {
        moveit::core::jointStateToRobotState(joint_states, *left_hand_robot_state_);
        robot_state::robotStateToRobotStateMsg(*left_hand_robot_state_, left_display_state_msg_.state);
        left_hand_robot_state_vis_pub_.publish(left_display_state_msg_);
    }
    else
    {
        moveit::core::jointStateToRobotState(joint_states, *right_hand_robot_state_);
        robot_state::robotStateToRobotStateMsg(*right_hand_robot_state_, right_display_state_msg_.state);
        right_hand_robot_state_vis_pub_.publish(right_display_state_msg_);
    }
}

int Base3DView::calcWristTarget(const geometry_msgs::PoseStamped& end_effector_pose, tf::Transform hand_T_palm, geometry_msgs::PoseStamped& final_pose)
{
    // Transform wrist_pose into the template pose frame
    tf::Transform ef_pose;
    tf::Transform target_pose;

    ef_pose.setRotation(tf::Quaternion(end_effector_pose.pose.orientation.x,end_effector_pose.pose.orientation.y,end_effector_pose.pose.orientation.z,end_effector_pose.pose.orientation.w));
    ef_pose.setOrigin(tf::Vector3(end_effector_pose.pose.position.x,end_effector_pose.pose.position.y,end_effector_pose.pose.position.z) );
    target_pose = ef_pose * hand_T_palm;

//    ROS_INFO("ef_pose: p=(%f, %f, %f) q=(%f, %f, %f, %f)",
//             ef_pose.getOrigin().getX(),ef_pose.getOrigin().getY(),ef_pose.getOrigin().getZ(),
//             ef_pose.getRotation().getW(),ef_pose.getRotation().getX(),ef_pose.getRotation().getY(),ef_pose.getRotation().getZ());

//    ROS_INFO("hand_T_palm: p=(%f, %f, %f) q=(%f, %f, %f, %f)",
//             hand_T_palm.getOrigin().getX(),hand_T_palm.getOrigin().getY(),hand_T_palm.getOrigin().getZ(),
//             hand_T_palm.getRotation().getW(),hand_T_palm.getRotation().getX(),hand_T_palm.getRotation().getY(),hand_T_palm.getRotation().getZ());

//    ROS_INFO("target_pose: p=(%f, %f, %f) q=(%f, %f, %f, %f)",
//             target_pose.getOrigin().getX(),target_pose.getOrigin().getY(),target_pose.getOrigin().getZ(),
//             target_pose.getRotation().getW(),target_pose.getRotation().getX(),target_pose.getRotation().getY(),target_pose.getRotation().getZ());

    tf::Quaternion tg_quat;
    tf::Vector3    tg_vector;
    tg_quat   = target_pose.getRotation();
    tg_vector = target_pose.getOrigin();

    final_pose.pose.orientation.w = tg_quat.getW();
    final_pose.pose.orientation.x = tg_quat.getX();
    final_pose.pose.orientation.y = tg_quat.getY();
    final_pose.pose.orientation.z = tg_quat.getZ();

    final_pose.pose.position.x = tg_vector.getX();
    final_pose.pose.position.y = tg_vector.getY();
    final_pose.pose.position.z = tg_vector.getZ();
    return 0;
}

void Base3DView::processLeftGhostHandPose(const geometry_msgs::PoseStamped::ConstPtr &pose)
{
    //ROS_INFO("LEFT GHOST HAND POSE:");
    //ROS_INFO("  position: %.2f %.2f %.2f",pose->pose.position.x,pose->pose.position.y,pose->pose.position.z);
    //ROS_INFO("  orientation: %.2f %.2f %.2f %.2f",pose->pose.orientation.w,pose->pose.orientation.x,pose->pose.orientation.y,pose->pose.orientation.z);
    if(!moving_pelvis_ && ghost_world_lock_[0] == 1)
    {
        geometry_msgs::Pose transformed_pose = pose->pose;
        staticTransform(transformed_pose, l_hand_T_palm_);
        end_effector_pose_list_["/l_arm_pose_marker"].pose = transformed_pose;
        publishGhostPoses();
    }
}

void Base3DView::processRightGhostHandPose(const geometry_msgs::PoseStamped::ConstPtr &pose)
{
    //ROS_INFO("RIGHT GHOST HAND POSE:");
    //ROS_INFO("  position: %.2f %.2f %.2f",pose->pose.position.x,pose->pose.position.y,pose->pose.position.z);
    //ROS_INFO("  orientation: %.2f %.2f %.2f %.2f",pose->pose.orientation.w,pose->pose.orientation.x,pose->pose.orientation.y,pose->pose.orientation.z);
    if(!moving_pelvis_ && ghost_world_lock_[1] == 1)
    {
        geometry_msgs::Pose transformed_pose = pose->pose;
        staticTransform(transformed_pose, r_hand_T_palm_);
        end_effector_pose_list_["/r_arm_pose_marker"].pose = transformed_pose;
        publishGhostPoses();
    }
}

void Base3DView::onMarkerFeedback(const flor_ocs_msgs::OCSInteractiveMarkerUpdate::ConstPtr& msg)//std::string topic_name, geometry_msgs::PoseStamped pose)
{
    geometry_msgs::PoseStamped joint_pose;
    joint_pose = msg->pose;

    //ROS_ERROR("Marker feedback on topic %s, have markers instantiated",msg->topic.c_str());
    if(msg->topic == "/l_arm_pose_marker")
    {
        ghost_planning_group_[0] = 1;
        ghost_planning_group_[1] = 0;
        moving_pelvis_ = false;
        moving_l_arm_ = true;
        moving_r_arm_ = false;

        //ROS_INFO("LEFT GHOST HAND POSE:");
        //ROS_INFO("  position: %.2f %.2f %.2f",msg->pose.pose.position.x,msg->pose.pose.position.y,msg->pose.pose.position.z);
        //ROS_INFO("  orientation: %.2f %.2f %.2f %.2f",msg->pose.pose.orientation.w,msg->pose.pose.orientation.x,msg->pose.pose.orientation.y,msg->pose.pose.orientation.z);
        calcWristTarget(msg->pose,l_hand_T_palm_.inverse(),joint_pose);
        publishHandPose(std::string("left"),joint_pose);
    }
    else if(msg->topic == "/r_arm_pose_marker")
    {
        ghost_planning_group_[0] = 0;
        ghost_planning_group_[1] = 1;
        moving_pelvis_ = false;
        moving_l_arm_ = false;
        moving_r_arm_ = true;

        ROS_INFO("RIGHT GHOST HAND POSE:");
        ROS_INFO("  position: %.2f %.2f %.2f",msg->pose.pose.position.x,msg->pose.pose.position.y,msg->pose.pose.position.z);
        ROS_INFO("  orientation: %.2f %.2f %.2f %.2f",msg->pose.pose.orientation.w,msg->pose.pose.orientation.x,msg->pose.pose.orientation.y,msg->pose.pose.orientation.z);
        calcWristTarget(msg->pose,r_hand_T_palm_.inverse(),joint_pose);
        publishHandPose(std::string("right"),joint_pose);
    }
    else if(msg->topic == "/pelvis_pose_marker")
    {
        moving_pelvis_ = true;
        moving_l_arm_ = false;
        moving_r_arm_ = false;
    }

    end_effector_pose_list_[msg->topic] = joint_pose; //msg->pose;

    if(marker_published_ < 3)
    {
        publishMarkers();

        if(msg->topic == "/l_arm_pose_marker" && marker_published_ == 0)
            marker_published_++;
        else if(msg->topic == "/r_arm_pose_marker" && marker_published_ == 1)
            marker_published_++;
        else if(msg->topic == "/pelvis_pose_marker" && marker_published_ == 2)
            marker_published_++;
    }

    //else
    //    ROS_ERROR("Marker feedback on topic %s, have no markers instantiated",msg->topic.c_str());
    publishGhostPoses();
}

void Base3DView::publishGhostPoses()
{
    bool left = ghost_planning_group_[0];
    bool right = ghost_planning_group_[1];
    bool torso = ghost_planning_group_[2];

    bool left_lock = ghost_world_lock_[0];
    bool right_lock = ghost_world_lock_[1];
    //ROS_ERROR("Moving marker? %s",moving_pelvis_?"Yes, Pelvis":(moving_l_arm_?"Yes, Left Arm":(moving_r_arm_?"Yes, Right Arm":"No")));
    //ROS_ERROR("Left lock: %s Right lock: %s",left_lock?"yes":"no",right_lock?"yes":"no");

    if(moving_pelvis_ || right_lock || left_lock)
    {
        if(!left_lock && left)
            left = false;
        else if(left_lock)
            left = true;
        if(!right_lock && right)
            right = false;
        else if(right_lock)
            right = true;
        moving_pelvis_ = false;

        if(moving_l_arm_ && right_lock)
        {
            left = true;
            right = false;
            torso = false;
            moving_l_arm_ = false;
        }
        if(moving_r_arm_ && left_lock)
        {
            left = false;
            right = true;
            torso = false;
            moving_l_arm_ = false;
        }
    }

    //ROS_ERROR("Left: %s Right:%s Pelvis: %s",left?"yes":"no",right?"yes":"no",torso?"yes":"no");

    flor_planning_msgs::TargetConfigIkRequest cmd;

    if(left && end_effector_pose_list_.find( "/l_arm_pose_marker") != end_effector_pose_list_.end())
    {
        cmd.target_poses.push_back(end_effector_pose_list_["/l_arm_pose_marker"]);
//        ROS_ERROR("PUBLISHING LEFT ARM POSE:");
//        ROS_ERROR("  position: %.2f %.2f %.2f",end_effector_pose_list_["/l_arm_pose_marker"].pose.position.x,end_effector_pose_list_["/l_arm_pose_marker"].pose.position.y,end_effector_pose_list_["/l_arm_pose_marker"].pose.position.z);
//        ROS_ERROR("  orientation: %.2f %.2f %.2f %.2f",end_effector_pose_list_["/l_arm_pose_marker"].pose.orientation.w,end_effector_pose_list_["/l_arm_pose_marker"].pose.orientation.x,end_effector_pose_list_["/l_arm_pose_marker"].pose.orientation.y,end_effector_pose_list_["/l_arm_pose_marker"].pose.orientation.z);
    }

    if(right && end_effector_pose_list_.find( "/r_arm_pose_marker") != end_effector_pose_list_.end())
    {
        cmd.target_poses.push_back(end_effector_pose_list_["/r_arm_pose_marker"]);
//        ROS_ERROR("PUBLISHING RIGHT ARM POSE:");
//        ROS_ERROR("  position: %.2f %.2f %.2f",end_effector_pose_list_["/r_arm_pose_marker"].pose.position.x,end_effector_pose_list_["/r_arm_pose_marker"].pose.position.y,end_effector_pose_list_["/r_arm_pose_marker"].pose.position.z);
//        ROS_ERROR("  orientation: %.2f %.2f %.2f %.2f",end_effector_pose_list_["/r_arm_pose_marker"].pose.orientation.w,end_effector_pose_list_["/r_arm_pose_marker"].pose.orientation.x,end_effector_pose_list_["/r_arm_pose_marker"].pose.orientation.y,end_effector_pose_list_["/r_arm_pose_marker"].pose.orientation.z);
    }

    cmd.lock_to_world.resize(cmd.target_poses.size());
    for(int i = 0; i < cmd.target_poses.size(); i++)
        cmd.lock_to_world[i].data = 0;//saved_state_world_lock_[i];

    if(left && !right && !torso)
        cmd.planning_group.data = "l_arm_group";
    else if(left && !right && torso)
        cmd.planning_group.data = "l_arm_with_torso_group";
    else if(!left && right && !torso)
        cmd.planning_group.data = "r_arm_group";
    else if(!left && right && torso)
        cmd.planning_group.data = "r_arm_with_torso_group";
    else if(left && right && !torso)
        cmd.planning_group.data = "both_arms_group";
    else if(left && right && torso)
        cmd.planning_group.data = "both_arms_with_torso_group";

    if(left || right)
        end_effector_pub_.publish(cmd);

    if(ghost_lock_pelvis_)
    {
        Ogre::Vector3 position(0,0,0);
        Ogre::Quaternion orientation(1,0,0,0);
        transform(position, orientation, "/pelvis", "/world");

        geometry_msgs::PoseStamped pose;
        pose.pose.position.x = position.x;
        pose.pose.position.y = position.y;
        pose.pose.position.z = position.z;
        pose.pose.orientation.x = orientation.x;
        pose.pose.orientation.y = orientation.y;
        pose.pose.orientation.z = orientation.z;
        pose.pose.orientation.w = orientation.w;
        pose.header.frame_id = "/world";
        pose.header.stamp = ros::Time::now();

        ghost_root_pose_pub_.publish(pose);

//        for(int i = 0; i < im_ghost_robot_server_.size(); i++)
//        {
//            if(im_ghost_robot_server_[i]->getMarkerName() == "Ghost Pelvis")
//            {
//                im_ghost_robot_server_[i]->setPose(pose);
//                break;
//            }
//        }

        flor_ocs_msgs::OCSInteractiveMarkerUpdate cmd;
        cmd.topic = "/pelvis_pose_marker";
        cmd.pose = pose;
        interactive_marker_update_pub_.publish(cmd);
    }
    else
    {
        // how do I set world lock for torso?
        ghost_root_pose_pub_.publish(end_effector_pose_list_["/pelvis_pose_marker"]);
    }
}

void Base3DView::processGhostControlState(const flor_ocs_msgs::OCSGhostControl::ConstPtr &msg)
{
    if(msg->snap)
    {
        snap_ghost_to_robot_ = true;
        return;
    }
    ghost_planning_group_.clear();
    ghost_pose_source_.clear();
    ghost_world_lock_.clear();

    ghost_planning_group_ = msg->planning_group;
    ghost_pose_source_ = msg->pose_source;
    ghost_world_lock_ = msg->world_lock;
    moveit_collision_avoidance_ = msg->collision_avoidance;
    ghost_lock_pelvis_ = msg->lock_pelvis;

    snap_ghost_to_robot_ = msg->snap;

    left_marker_moveit_loopback_ = msg->left_moveit_marker_loopback;
    right_marker_moveit_loopback_ = msg->right_moveit_marker_loopback;
}

void Base3DView::processJointStates(const sensor_msgs::JointState::ConstPtr &states)
{
    if(snap_ghost_to_robot_)
    {
        ghost_joint_state_pub_.publish(states);

        Ogre::Vector3 position(0,0,0);
        Ogre::Quaternion orientation(1,0,0,0);
        transform(position, orientation, "/pelvis", "/world");

        geometry_msgs::PoseStamped pose;
        pose.pose.position.x = position.x;
        pose.pose.position.y = position.y;
        pose.pose.position.z = position.z;
        pose.pose.orientation.x = orientation.x;
        pose.pose.orientation.y = orientation.y;
        pose.pose.orientation.z = orientation.z;
        pose.pose.orientation.w = orientation.w;
        pose.header.frame_id = "/world";
        pose.header.stamp = ros::Time::now();

        ghost_root_pose_pub_.publish(pose);

        flor_ocs_msgs::OCSInteractiveMarkerUpdate cmd;
        cmd.topic = "/pelvis_pose_marker";
        cmd.pose = pose;
        interactive_marker_update_pub_.publish(cmd);

        snap_ghost_to_robot_ = false;
    }

}

void Base3DView::processPelvisResetRequest( const std_msgs::Bool::ConstPtr &msg )
{
    if(end_effector_pose_list_.find( "/pelvis_pose_marker") != end_effector_pose_list_.end())
    {
        // trasnfrom current marker pose from world to pelvis
        geometry_msgs::PoseStamped marker_pose = end_effector_pose_list_["/pelvis_pose_marker"];
        Ogre::Quaternion original_orientation(marker_pose.pose.orientation.w,marker_pose.pose.orientation.x,marker_pose.pose.orientation.y,marker_pose.pose.orientation.z);
        Ogre::Vector3 position(0,0,0);
        Ogre::Quaternion orientation(1,0,0,0);
        transform(position, orientation, "/pelvis", "/world");

        //ROS_ERROR("ROTATION PELVIS: %f %f %f",orientation.getPitch().valueDegrees(),orientation.getYaw().valueDegrees(),orientation.getRoll().valueDegrees());
        //ROS_ERROR("ROTATION GHOST : %f %f %f",original_orientation.getPitch().valueDegrees(),original_orientation.getYaw().valueDegrees(),original_orientation.getRoll().valueDegrees());

        Ogre::Quaternion final_orientation = Ogre::Quaternion(original_orientation.getRoll(), Ogre::Vector3::UNIT_Z) * Ogre::Quaternion(orientation.getPitch(), Ogre::Vector3::UNIT_X) * Ogre::Quaternion(orientation.getYaw(), Ogre::Vector3::UNIT_Y);

        // create pose based on /pelvis frame
        geometry_msgs::PoseStamped pose;// = end_effector_pose_list_["/pelvis_pose_marker"];
        pose.pose.position.x = marker_pose.pose.position.x;
        pose.pose.position.y = marker_pose.pose.position.y;
        pose.pose.position.z = marker_pose.pose.position.z;
        pose.pose.orientation.x = final_orientation.x;
        pose.pose.orientation.y = final_orientation.y;
        pose.pose.orientation.z = final_orientation.z;
        pose.pose.orientation.w = final_orientation.w;
        pose.header.frame_id = "/world";
        pose.header.stamp = ros::Time::now();

        ghost_root_pose_pub_.publish(pose);

//        for(int i = 0; i < im_ghost_robot_server_.size(); i++)
//        {
//            if(im_ghost_robot_server_[i]->getMarkerName() == "Ghost Pelvis")
//            {
//                im_ghost_robot_server_[i]->setPose(pose);
//                break;
//            }
//        }

        flor_ocs_msgs::OCSInteractiveMarkerUpdate cmd;
        cmd.topic = "/pelvis_pose_marker";
        cmd.pose = pose;
        interactive_marker_update_pub_.publish(cmd);
    }
}

void Base3DView::processSendPelvisToFootstepRequest( const std_msgs::Bool::ConstPtr& msg )
{
    send_footstep_goal_pub_.publish(end_effector_pose_list_["/pelvis_pose_marker"]);
}

void Base3DView::publishMarkers()
{
    geometry_msgs::Point point;

    flor_ocs_msgs::OCSInteractiveMarkerAdd marker_server_left_arm;
    marker_server_left_arm.name = "Ghost Left Arm";
    marker_server_left_arm.topic = "/l_arm_pose_marker";
    marker_server_left_arm.frame = manager_->getFixedFrame().toStdString();
    marker_server_left_arm.scale = 0.2;
    marker_server_left_arm.point = point;
    interactive_marker_add_pub_.publish(marker_server_left_arm);

    flor_ocs_msgs::OCSInteractiveMarkerAdd marker_server_right_arm;
    marker_server_right_arm.name = "Ghost Right Arm";
    marker_server_right_arm.topic = "/r_arm_pose_marker";
    marker_server_right_arm.frame = manager_->getFixedFrame().toStdString();
    marker_server_right_arm.scale = 0.2;
    marker_server_right_arm.point = point;
    interactive_marker_add_pub_.publish(marker_server_right_arm);

    flor_ocs_msgs::OCSInteractiveMarkerAdd marker_server_pelvis;
    marker_server_pelvis.name = "Ghost Pelvis";
    marker_server_pelvis.topic = "/pelvis_pose_marker";
    marker_server_pelvis.frame = manager_->getFixedFrame().toStdString();
    marker_server_pelvis.scale = 0.2;
    marker_server_pelvis.point = point;
    interactive_marker_add_pub_.publish(marker_server_pelvis);
}

void Base3DView::resetView()
{
    //ROS_ERROR("RESET VIEW");
    getCurrentViewController()->reset();
    Ogre::Vector3 position(0,0,0);
    Ogre::Quaternion orientation(1,0,0,0);
    transform(position, orientation, "/pelvis", "/world");
    getCurrentViewController()->lookAt(position);
    //if(dynamic_cast<rviz::OrbitViewController*>(manager_->getViewManager()->getCurrent()) == NULL)
    //    ((rviz::OrbitViewController*)manager_->getViewManager()->getCurrent())->lookAt(position);
    //else if(dynamic_cast<rviz::FixedOrientationOrthoViewController*>(manager_->getViewManager()->getCurrent()) == NULL)
    //    ((rviz::FixedOrientationOrthoViewController*)manager_->getViewManager()->getCurrent())->lookAt(position);
}

void Base3DView::clearPointCloudRequests()
{
    point_cloud_request_viewer_->setEnabled(false);
    point_cloud_request_viewer_->setEnabled(true);
}

void Base3DView::clearMapRequests()
{
    while(ground_map_.size() > 0)
    {
        rviz::Display* tmp = ground_map_[0];
        tmp->setEnabled(false);
        delete tmp;
        manager_->notifyConfigChanged();
        ground_map_.erase(ground_map_.begin());
    }
}

rviz::ViewController* Base3DView::getCurrentViewController()
{
     return manager_->getViewManager()->getCurrent();
}

}

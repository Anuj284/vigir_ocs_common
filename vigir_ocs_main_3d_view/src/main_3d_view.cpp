/*
 * Main3DView class implementation.
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

#include "rviz/visualization_manager.h"
//#include "rviz/render_panel.h"
#include <render_panel_custom.h>
#include "rviz/display.h"
#include "rviz/frame_manager.h"
#include "rviz/tool_manager.h"
#include <selection_handler.h>
#include "main_3d_view.h"

#include "flor_ocs_msgs/OCSTemplateAdd.h"

#include "flor_ocs_msgs/OCSWaypointAdd.h"

// Constructor for Main3DView.  This does most of the work of the class.
Main3DView::Main3DView( QWidget* parent )
    : QWidget( parent )
{
    // Create a new label for this widget.
    //QLabel* robot_model_label = new QLabel( "rviz/RobotModel" );

    // Construct and lay out render panel.
    render_panel_ = new rviz::RenderPanelCustom();
    QVBoxLayout* main_layout = new QVBoxLayout;
    //main_layout->addWidget( robot_model_label );
    main_layout->addWidget( render_panel_ );

    // Set the top-level layout for this MyViz widget.
    setLayout( main_layout );

    // Make signal/slot connections.
    //connect( collision_checkbox, SIGNAL( valueChanged( int )), this, SLOT( setCollision( bool )));

    // Next we initialize the main RViz classes.
    //
    // The VisualizationManager is the container for Display objects,
    // holds the main Ogre scene, holds the ViewController, etc.  It is
    // very central and we will probably need one in every usage of
    // librviz.
    manager_ = new rviz::VisualizationManager( render_panel_ );
    render_panel_->initialize( manager_->getSceneManager(), manager_ );
    manager_->initialize();
    manager_->startUpdate();

    // Create a RobotModel display.
    robot_model_ = manager_->createDisplay( "rviz/RobotModel", "Robot model", true );
    ROS_ASSERT( robot_model_ != NULL );

    // Set topic that will be used as 0,0,0 -> reference for all the other transforms
    // IMPORTANT: WITHOUT THIS, ALL THE DIFFERENT PARTS OF THE ROBOT MODEL WILL BE DISPLAYED AT 0,0,0
    manager_->setFixedFrame("/pelvis");

    // Add support for interactive markers
    interactive_markers_tool_ = manager_->getToolManager()->addTool( "rviz/Interact" );
    // Add support for selection
    selection_tool_ = manager_->getToolManager()->addTool( "rviz/Select" );
    // Add support for camera movement
    move_camera_tool_ = manager_->getToolManager()->addTool( "rviz/MoveCamera" );
    // Add support for goal specification/vector navigation
    set_goal_tool_ = manager_->getToolManager()->addTool( "rviz/SetGoal" );

    // Add interactive markers Stefan's markers and IK implementation
    interactive_marker_robot_[0] = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker 1", true );
    interactive_marker_robot_[0]->subProp( "Update Topic" )->setValue( "/l_arm_pose_marker/update" );
    interactive_marker_robot_[1] = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker 2", true );
    interactive_marker_robot_[1]->subProp( "Update Topic" )->setValue( "/l_leg_pose_marker/update" );
    interactive_marker_robot_[2] = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker 3", true );
    interactive_marker_robot_[2]->subProp( "Update Topic" )->setValue( "/r_arm_pose_marker/update" );
    interactive_marker_robot_[3] = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker 4", true );
    interactive_marker_robot_[3]->subProp( "Update Topic" )->setValue( "/r_leg_pose_marker/update" );

    // Add template marker
    interactive_marker_template_ = manager_->createDisplay( "rviz/InteractiveMarkers", "Interactive marker template", true );
    interactive_marker_template_->subProp( "Update Topic" )->setValue( "/template_pose_marker/update" );

    // Make the move camera tool the currently selected one
    manager_->getToolManager()->setCurrentTool( move_camera_tool_ );

    // Create a LaserScan display.
    laser_scan_ = manager_->createDisplay( "rviz/LaserScan", "Laser Scan", false );
    ROS_ASSERT( laser_scan_ != NULL );

    laser_scan_->subProp( "Topic" )->setValue( "/multisense_sl/laser/scan" );
    laser_scan_->subProp( "Size (m)" )->setValue( 0.1 );
    laser_scan_->subProp( "Decay Time" )->setValue( 1 );

    // Create a MarkerArray display.
    //marker_array_ = manager_->createDisplay( "rviz/MarkerArray", "MarkerArray", true );
    marker_array_ = manager_->createDisplay( "rviz/OctomapDisplayCustom", "Octomap", true );
    ROS_ASSERT( marker_array_ != NULL );

    marker_array_->subProp( "Marker Topic" )->setValue( "/worldmodel_main/occupied_cells_vis_array" );

    // Create a point cloud display.
    stereo_point_cloud_viewer_ = manager_->createDisplay( "rviz/PointCloud2", "Point Cloud", false );
    ROS_ASSERT( stereo_point_cloud_viewer_ != NULL );
    stereo_point_cloud_viewer_->subProp( "Style" )->setValue( "Points" );
    stereo_point_cloud_viewer_->subProp( "Topic" )->setValue( "/multisense_sl/camera/points2" );
    stereo_point_cloud_viewer_->subProp( "Size (Pixels)" )->setValue( 3 );

    lidar_point_cloud_viewer_ = manager_->createDisplay( "rviz/PointCloud2", "Point Cloud", true );
    ROS_ASSERT( lidar_point_cloud_viewer_ != NULL );
    lidar_point_cloud_viewer_->subProp( "Style" )->setValue( "Points" );
    lidar_point_cloud_viewer_->subProp( "Topic" )->setValue( "/scan_cloud_filtered" );
    lidar_point_cloud_viewer_->subProp( "Size (Pixels)" )->setValue( 3 );
    template_display_ = manager_->createDisplay( "rviz/TemplateDisplayCustom", "Template Display", true );

    selection_3d_display_ = manager_->createDisplay( "rviz/Selection3DDisplayCustom", "3D Selection Display", true );
    
    waypoints_display_ = manager_->createDisplay( "rviz/PathDisplayCustom", "Path Display", true );
    waypoints_display_->subProp( "Topic" )->setValue( "/waypoint/list" );

    // connect the 3d selection tool to its display
    //QObject::connect(selection_3d_tool_, SIGNAL(select(int,int,int,int)), selection_3d_display_, SLOT(createMarker(int,int,int,int)));
    //QObject::connect(this, SIGNAL(rightClickEvent(int,int)), selection_3d_display_, SLOT(createMarker(int,int)));
    QObject::connect(this, SIGNAL(setRenderPanel(rviz::RenderPanel*)), selection_3d_display_, SLOT(setRenderPanel(rviz::RenderPanel*)));
    QObject::connect(selection_3d_display_, SIGNAL(newSelection(Ogre::Vector3)), this, SLOT(newSelection(Ogre::Vector3)));

    Q_EMIT setRenderPanel(this->render_panel_);

    // Set topic that will be used as 0,0,0 -> reference for all the other transforms
    // IMPORTANT: WITHOUT THIS, ALL THE DIFFERENT PARTS OF THE ROBOT MODEL WILL BE DISPLAYED AT 0,0,0
    manager_->getFrameManager()->setFixedFrame("/pelvis");
    
    // handles selections without rviz::tool
    selection_handler_ = new vigir_ocs::SelectionHandler();
    QObject::connect(render_panel_, SIGNAL(signalMousePressEvent(QMouseEvent*)), selection_handler_, SLOT(mousePressEvent(QMouseEvent*)));
    QObject::connect(selection_handler_, SIGNAL(select(int,int)), selection_3d_display_, SLOT(createMarker(int,int)));
    QObject::connect(selection_handler_, SIGNAL(selectROI(int,int)), selection_3d_display_, SLOT(createROISelection(int,int)));

    // create a publisher to add templates
    template_add_pub_   = n_.advertise<flor_ocs_msgs::OCSTemplateAdd>( "/template/add", 1, false );
    
	// create a publisher to add waypoints
    waypoint_add_pub_   = n_.advertise<flor_ocs_msgs::OCSWaypointAdd>( "/waypoint/add", 1, false );
}

// Destructor.
Main3DView::~Main3DView()
{
    delete manager_;
}

void Main3DView::robotModelToggled( bool selected )
{
    robot_model_->setEnabled( selected );
}

void Main3DView::lidarPointCloudToggled( bool selected )
{
    lidar_point_cloud_viewer_->setEnabled( selected );
}

void Main3DView::stereoPointCloudToggled( bool selected )
{
    stereo_point_cloud_viewer_->setEnabled( selected );
}

void Main3DView::laserScanToggled( bool selected )
{
    laser_scan_->setEnabled( selected );
}

void Main3DView::markerArrayToggled( bool selected )
{
    marker_array_->setEnabled( selected );
}

void Main3DView::cameraToggled( bool selected )
{
    if(selected)
        manager_->getToolManager()->setCurrentTool( move_camera_tool_ );
}

void Main3DView::selectToggled( bool selected )
{
    if(selected)
        manager_->getToolManager()->setCurrentTool( selection_tool_ );
}

void Main3DView::select3DToggled( bool selected )
{
    //if(selected)
    //    manager_->getToolManager()->setCurrentTool( selection_3d_tool_ );
}

void Main3DView::markerRobotToggled( bool selected )
{
    if(selected)
    {
        manager_->getToolManager()->setCurrentTool( interactive_markers_tool_ );

        // enable robot IK markers
        for( int i = 0; i < 4; i++ )
        {
            interactive_marker_robot_[i]->setEnabled( true );
        }
        // disable template marker
        interactive_marker_template_->setEnabled( false );
    }
}

void Main3DView::markerTemplateToggled( bool selected )
{
    if(selected)
    {
        manager_->getToolManager()->setCurrentTool( interactive_markers_tool_ );

        // disable robot IK markers
        for( int i = 0; i < 4; i++ )
        {
            interactive_marker_robot_[i]->setEnabled( false );
        }
        // enable template markers
        interactive_marker_template_->setEnabled( true );
    }
}

void Main3DView::vectorToggled( bool selected )
{
    if(selected)
        manager_->getToolManager()->setCurrentTool( set_goal_tool_ );
}

void Main3DView::newSelection( Ogre::Vector3 position )
{
    selection_position_ = position;
}

void Main3DView::insertTemplate( QString path )
{
    std::cout << "adding template" << std::endl;

    flor_ocs_msgs::OCSTemplateAdd cmd;
    geometry_msgs::PoseStamped pose;

    cmd.template_path = path.toStdString();

    pose.pose.position.x = selection_position_.x;
    pose.pose.position.y = selection_position_.y;
    pose.pose.position.z = selection_position_.z;
    pose.pose.orientation.x = 0;
    pose.pose.orientation.y = 0;
    pose.pose.orientation.z = 1;
    pose.pose.orientation.w = 0;
    
    pose.header.frame_id = "/pelvis";
    cmd.pose = pose;

    // publish complete list of templates and poses
    template_add_pub_.publish( cmd );
}

void Main3DView::insertWaypoint()
{
    std::cout << "adding waypoint" << std::endl;
    
    flor_ocs_msgs::OCSWaypointAdd cmd;
    geometry_msgs::PoseStamped pose;

    pose.pose.position.x = selection_position_.x;
    pose.pose.position.y = selection_position_.y;
    pose.pose.position.z = selection_position_.z;
    pose.pose.orientation.x = 0;
    pose.pose.orientation.y = 0;
    pose.pose.orientation.z = 1;
    pose.pose.orientation.w = 0;
    
    pose.header.frame_id = "/pelvis";
    cmd.pose = pose;

    // publish complete list of templates and poses
    waypoint_add_pub_.publish( cmd );
}

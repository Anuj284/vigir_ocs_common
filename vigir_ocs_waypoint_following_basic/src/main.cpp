#include "ros/ros.h"
#include <nodelet/loader.h>

int main(int argc, char **argv)
{
    ros::init(argc, argv, "vigir_ocs_waypoint_following_basic");

    nodelet::Loader manager(true);
    nodelet::M_string remappings;
    nodelet::V_string my_argv;

    manager.load(ros::this_node::getName(), "vigir_ocs_waypoint_following_basic/WaypointFollowing", remappings, my_argv);

    ros::spin();
    return 0;
}

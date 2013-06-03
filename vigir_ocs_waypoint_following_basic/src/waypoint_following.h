#include <ros/ros.h>
#include <ros/timer.h>
#include <nodelet/nodelet.h>
#include <pluginlib/class_list_macros.h>

#include <boost/thread.hpp>
#include <vector>
#include <string>


#include <flor_ocs_msgs/OCSWaypointUpdate.h>

#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/Imu.h>
#include <nav_msgs/Path.h>

namespace ocs_waypoint
{
    class WaypointFollowingBasic : public nodelet::Nodelet
    {
      public:
        virtual void onInit();

        void receivedUpdateWaypointMessage( const nav_msgs::Path::ConstPtr& msg);
        void receivedRobotLocUpdate( const nav_msgs::Odometry::ConstPtr& msg);

      protected:
        ros::Subscriber waypoint_update;
        ros::Subscriber robot_loc;
        ros::Publisher remove_pub_;
        ros::Publisher drive_pub_;
        ros::NodeHandle nh;


      private:
        bool atWaypoint();
        void endOfList();
        float oldTurn;
        bool pointsClose(float x1, float x2);
        void moveRobot(const ros::TimerEvent&);
        int findClosestWaypoint(int maxDist);
        ros::Timer timer;
        int maxSpeed;
        nav_msgs::Path waypoints;
        int destWaypoint;
        double maxTurn;
        float robotX;
        float robotY;
        double robotHeading;
    };
}

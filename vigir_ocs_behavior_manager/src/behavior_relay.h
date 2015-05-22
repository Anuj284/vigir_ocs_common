#ifndef BEHAVIOR_RELAY_H
#define BEHAVIOR_RELAY_H

#include <stdlib.h>
#include <ros/ros.h>
#include "flor_ocs_msgs/OCSOverlayText.h"
#include "flor_ocs_msgs/OCSBehaviorGoal.h"
#include "behavior_notification.h"
#include <boost/asio/ip/host_name.hpp>

//#include <actionlib/server/simple_action_server.h>
#include <vigir_be_msgs/BehaviorInputAction.h>
//#include "complex_action_server.h"

/**
 * Subscribe to different topics in order to process whether a required action has been processed.
 *
 * Controls positioning of individual notifications
 *
 */


/**
 server is defined in the behavior notification to circumvent circular dependency
 typedef actionlib::SimpleActionServer<vigir_be_msgs::BehaviorInputAction> BehaviorServer;
**/

Q_DECLARE_METATYPE(BehaviorServer::GoalHandle);

class BehaviorRelay: public QWidget
{
    Q_OBJECT

   public:
       explicit BehaviorRelay(QWidget *parent = 0);
       std::vector<BehaviorNotification*> getNotifications();
       int getMaxNotificationsShown(){return max_notifications_shown_;}

   private:
       BehaviorRelay(BehaviorRelay const&){};             // copy constructor is private
       BehaviorRelay& operator=(BehaviorRelay const&){};  // assignment operator is private
       void receiveBehaviorGoalCB(const flor_ocs_msgs::OCSBehaviorGoalConstPtr& msg);
       void receiveBehaviorResult(const flor_ocs_msgs::OCSBehaviorGoalConstPtr& msg);
       void cleanNotifications();

       boost::recursive_mutex lock_;

       QWidget* parent_;
       ros::NodeHandle nh_;
       std::vector<BehaviorNotification*> behavior_notifications_;
       int max_notifications_shown_;
       BehaviorServer* behavior_server_;
       QString latest_behavior_action_text_;

       std::vector<BehaviorServer::GoalHandle> all_goals_;

       ros::Subscriber behavior_goal_sub_;
       ros::Subscriber behavior_confirm_sub_;
       ros::Publisher  behavior_confirm_pub_;

Q_SIGNALS:
       void updateUI();
       void signalCreateNotification(QString);

public Q_SLOTS:
    void reportConfirmation(QString, int id);
    void reportAbort(QString action_text, int id);
    void createNotification(QString action_text, int id, int goal_type);
};



#endif //BEHAVIOR_RELAY_H

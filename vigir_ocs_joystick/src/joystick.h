/* 
 * Joystick class definition.
 * 
 * Author: Felipe Bacim.
 * 
 * Based on librviz_tutorials and the pr2 teleop tutorial.
 * 
 * Latest changes (12/08/2012):
 * - created class
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <QWidget>
#include <QtGui>

#include "flor_ocs_msgs/OCSDrive.h"
#include <ros/publisher.h>
#include <ros/subscriber.h>
#include <ros/ros.h>

//#include "ui/joystick_widget.h"

// Class "Joystick" implements the widget that can send OCSDrive messages to the robot to move it.
class Joystick: public QWidget
{
    Q_OBJECT

public:
    explicit Joystick( QWidget* parent = 0 )/* : robot_steer(0), robot_throttle(0){}*/;
    virtual ~Joystick();

    void setRobotThrottle( unsigned char throttle );
    unsigned char getRobotThrottle();
    void setRobotSteer( char steer );
    char getRobotSteer();
    void publish();

    // callback
    void JoystickFeedbackCB(const flor_ocs_msgs::OCSDrive::ConstPtr &str);

Q_SIGNALS:
    void throttleUpdated(unsigned char);

private:
    flor_ocs_msgs::OCSDrive drive_cmd;

    char robot_steer;
    unsigned char robot_throttle;

    ros::NodeHandle n_;
    ros::Publisher drive_pub_;

    ros::Subscriber sub_;

protected:
    void timerEvent(QTimerEvent *event);
private:
    QBasicTimer timer;
};
#endif // JOYSTICK_H

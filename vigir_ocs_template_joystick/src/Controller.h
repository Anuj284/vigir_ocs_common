/*
 * RobotModel class definition.
 *
 * Author: Brian Wright
 *
 * Based on librviz_tutorials and the .
 *
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <ros/ros.h>
#include <QWidget>
#include <QQuaternion>
#include <vigir_ocs_msgs/OCSTemplateList.h>
#include <vigir_ocs_msgs/OCSControlMode.h>
#include <vigir_ocs_msgs/OCSCameraTransform.h>
#include <vigir_ocs_msgs/OCSTemplateUpdate.h>
#include <vigir_ocs_msgs/OCSInteractiveMarkerUpdate.h>
#include <sensor_msgs/Joy.h>
#include <sensor_msgs/JoyFeedbackArray.h>
#include <string>
#include <QBasicTimer>
#include <geometry_msgs/PoseStamped.h>
#include <flor_planning_msgs/TargetConfigIkRequest.h>

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>


namespace vigir_ocs
{
// Class "Main3DView" implements the RobotModel class with joint manipulation that can be added to any QT application.
class Controller: public QWidget
{
    Q_OBJECT
public:
    Controller( QWidget* parent = 0 );
    virtual ~Controller();
    void templateListCb(const vigir_ocs_msgs::OCSTemplateList::ConstPtr& msg);
    void buildmsg(float posX, float posZ , float rotY, float rotX);
    void joyCB(const sensor_msgs::Joy::ConstPtr& msg);
    void leftCB(const geometry_msgs::PoseStamped::ConstPtr& msg);
    void rightCB(const geometry_msgs::PoseStamped::ConstPtr& msg);
    void modeCb(const vigir_ocs_msgs::OCSControlMode::ConstPtr& msg);
    void changeTemplate();
    std::vector<std::string> getTemplateNames();
    void cameraCb(const vigir_ocs_msgs::OCSCameraTransform::ConstPtr& msg);
    int getManipulation();
    int getObjectMode();

protected:
    ros::Subscriber joystick_modes_sub;
    ros::Publisher joystick_modes_pub;
    ros::Subscriber camera_sub;
    ros::Subscriber template_list_sub;
    ros::Publisher template_update_pub;
    ros::Subscriber joy_sub;
    ros::Publisher joy_pub;
    ros::Subscriber left_sub;
    ros::Subscriber right_sub;
    ros::Publisher ghost_hand_pub;
    vigir_ocs_msgs::OCSTemplateList temList;
    vigir_ocs_msgs::OCSControlMode joyModes;
    vigir_ocs_msgs::OCSCameraTransform cameraUpdate;
    sensor_msgs::Joy joy;
    sensor_msgs::Joy oldJoy;
    geometry_msgs::PoseStamped leftHand;
    geometry_msgs::PoseStamped rightHand;
    void timerEvent(QTimerEvent *event);

private:
   ros::NodeHandle nh;
   QBasicTimer timer;
   int templateIndex;
   bool initialPublish;
   bool leftMode;
   bool rightMode;
   bool worldMode;
   bool objectMode;
   void buildTransformation(float posX, float posY ,float posZ, float rotX, float rotY,float rotZ, QQuaternion* rotation, QVector3D* position);
   void handleJoystick();
   bool compareJoyData();
   void handleButtons();
   void buildJoy();
   void handleArms();
   QVector3D cameraPosition;
   QQuaternion cameraOrientation;
   int currentManipulationController;
   void changeManipulationController();
   int currentObjectMode;


Q_SIGNALS:
   void updateTemplateComboBox(int tempID);
   void updateUIModes(int,int);

public Q_SLOTS:
    void changeTemplateID(int newID);
    void setObjectMode(int mode);
    void setManipulation(int mode);


};
}
#endif // Controller_H

#ifndef GLANCEHUB_H
#define GLANCEHUB_H

#include <vigir_control_msgs/VigirControlModeCommand.h>
#include <vigir_control_msgs/VigirControlMode.h>
#include <vigir_ocs_msgs/OCSRobotStatus.h>
#include <vigir_ocs_msgs/OCSFootstepStatus.h>

#include <ros/subscriber.h>

#include <vigir_footstep_planning_msgs/footstep_planning_msgs.h>

#include <QMainWindow>
#include <QBasicTimer>
#include <QTableWidgetItem>
#include <QTableWidget>

namespace Ui {
class glancehub;
}


class glancehub : public QMainWindow
{
    Q_OBJECT

public:
    explicit glancehub(QWidget *parent = 0);
    ~glancehub();
    void controlModeMsgRcv(const vigir_control_msgs::VigirControlMode::ConstPtr msg);
    void robotStatusMoveit(const vigir_ocs_msgs::OCSRobotStatus::ConstPtr msg);
    //void robotStatusFootstep(const vigir_ocs_msgs::OCSRobotStatus::ConstPtr msg);
    void robotStatusFootstepComplete(const vigir_ocs_msgs::OCSFootstepStatus::ConstPtr msg);
    void loadFile();
    QString timeFromMsg(ros::Time stamp);
    QString getMoveitStat();
    QString getFootstepStat();

protected:
    void timerEvent(QTimerEvent *event);

private:
    Ui::glancehub *ui;
    ros::Subscriber control_mode_sub_;
    ros::Subscriber moveit_status_sub_;
    ros::Subscriber footstep_status_simple_sub_;
    ros::Subscriber footstep_status_sub_;
    ros::Subscriber obfsm_footstep_status_sub_;
    QBasicTimer timer;

    std::vector<std::string> allowed_control_modes_;

    std::vector<std::string> errors;
    std::string messagesPath;

Q_SIGNALS:
    void sendMoveitStatus(bool);
    void sendFootstepStatus(int);
    void sendFlorStatus(int);


};

#endif // glancehub_H

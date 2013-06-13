#ifndef BandwidthWidget_H
#define BandwidthWidget_H


#include <QMainWindow>
#include <QWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>
#include <QStringList>

#include <QPainter>
#include <QtGui>

#include <flor_ocs_msgs/OCSBandwidth.h>
#include <flor_ocs_msgs/VRCdata.h>
#include <ros/ros.h>
#include <std_msgs/String.h>

namespace Ui {
class BandwidthWidget;
}

class BandwidthWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BandwidthWidget(QWidget *parent = 0);
    ~BandwidthWidget();

    void processBandwidthMessage(const flor_ocs_msgs::OCSBandwidth::ConstPtr& msg);
    void processVRCData(const flor_ocs_msgs::VRCdata::ConstPtr& msg);
    void heartbeatRecieved(const std_msgs::String::ConstPtr& msg);
private:
    Ui::BandwidthWidget* ui;

    ros::NodeHandle nh_;
    ros::Subscriber ocs_bandwidth_sub_;
    ros::Subscriber topic_heartbeat_sub_;
    ros::Subscriber vrc_data_sub_;

    typedef struct
    {
        std::string node_name;
        unsigned long total_bytes_read;
        unsigned long total_bytes_sent;
    } BandwidthStruct;

    std::vector<BandwidthStruct> node_bandwidth_info_;

    QTableWidgetItem* total_bytes_read_item;
    int foobar;
    QTableWidgetItem* total_bytes_sent_item;
    bool bytes_remaining_initialized;
    int32_t down_max;
    int32_t up_max;
    typedef struct
    {
        QBasicTimer timer;
        int alarmsSinceReturn;
    }topicObject;
    std::vector<topicObject*> topicObjectsList;


protected:
    void timerEvent(QTimerEvent *event);
private:
    QBasicTimer timer;
};

#endif // BandwidthWidget_H

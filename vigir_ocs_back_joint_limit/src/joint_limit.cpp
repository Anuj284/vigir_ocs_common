#include "joint_limit.h"
#include "ui_joint_limit.h"
#include <ros/package.h>
#include <flor_planning_msgs/PlannerConfiguration.h>

joint_limit::joint_limit(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::joint_limit)
{
    ui->setupUi(this);
    constraints_pub_ = nh_.advertise<flor_planning_msgs::PlannerConfiguration>( "/flor/planning/upper_body/configuration",1,false);
    lbzMinVal = -0.610865;
    lbzMaxVal = 0.610865;

    mbyMinVal = -1.2;
    mbyMaxVal = 1.28;

    ubxMinVal = -0.790809;
    ubxMaxVal = 0.790809;
    key_event_sub_ = nh_.subscribe<flor_ocs_msgs::OCSKeyEvent>( "/flor/ocs/key_event", 5, &joint_limit::processNewKeyEvent, this );

    timer.start(33, this);
}

joint_limit::~joint_limit()
{
    delete ui;
}
void joint_limit::on_lbzMin_sliderReleased()
{
    if(ui->lbzMin->value() >= lbzMaxVal*1000000.0)
        ui->lbzMin->setValue(lbzMinVal*1000000.0);
    else
    {
        lbzMinVal = (float)ui->lbzMin->value()/1000000.0;
        ui->lbzMinLabel->setText(QString::number(lbzMinVal,'g',6));
    }
}

void joint_limit::on_lbzMax_sliderReleased()
{
    if(ui->lbzMax->value() <= lbzMinVal*1000000.0)
        ui->lbzMax->setValue(lbzMaxVal*1000000.0);
    else
    {
        lbzMaxVal = (float)ui->lbzMax->value()/1000000.0;
        ui->lbzMaxLabel->setText(QString::number(lbzMaxVal,'g',6));
    }
}

void joint_limit::on_mbyMin_sliderReleased()
{
    if(ui->mbyMin->value() >= mbyMaxVal*100.0)
        ui->mbyMin->setValue(mbyMinVal*100.0);
    else
    {
        mbyMinVal = (float)ui->mbyMin->value()/100.0;
        ui->mbyMinLabel->setText(QString::number(mbyMinVal,'g',6));
    }
}

void joint_limit::on_mbyMax_sliderReleased()
{
    if(ui->mbyMax->value() <= mbyMinVal*100.0)
        ui->mbyMax->setValue(mbyMaxVal*100.0);
    else
    {
        mbyMaxVal = (float)ui->mbyMax->value()/100.0;
        ui->mbyMaxLabel->setText(QString::number(mbyMaxVal,'g',6));
    }
}

void joint_limit::on_ubxMin_sliderReleased()
{
    if(ui->ubxMin->value() >= ubxMaxVal*1000000.0)
        ui->ubxMin->setValue(ubxMinVal*1000000.0);
    else
    {
        ubxMinVal = (float)ui->ubxMin->value()/1000000.0;
        ui->ubxMinLabel->setText(QString::number(ubxMinVal,'g',6));
    }
}

void joint_limit::on_ubxMax_sliderReleased()
{
    if(ui->ubxMax->value() <= ubxMinVal*1000000.0)
        ui->ubxMax->setValue(ubxMaxVal*1000000.0);
    else
    {
        ubxMaxVal = (float)ui->ubxMax->value()/1000000.0;
        ui->ubxMaxLabel->setText(QString::number(ubxMaxVal,'g',6));
    }
}

void joint_limit::on_apply_clicked()
{
    flor_planning_msgs::PlannerConfiguration msg;

    msg.joint_position_constraints.back_bkz_max.data = (float)lbzMaxVal;
    msg.joint_position_constraints.back_bkz_min.data = (float)lbzMinVal;

    msg.joint_position_constraints.back_bky_max.data = (float)mbyMaxVal;
    msg.joint_position_constraints.back_bky_min.data = (float)mbyMinVal;

    msg.joint_position_constraints.back_bkx_max.data = (float)ubxMaxVal;
    msg.joint_position_constraints.back_bkx_min.data = (float)ubxMinVal;
    std::cout << "The following values were set:" <<std::endl;
    std::cout << "lbz: max = " << ui->lbzMax->value() << " min = " << ui->lbzMin->value() << std::endl;
    std::cout << "mby: max = " << ui->mbyMax->value() << " min = " << ui->mbyMin->value() << std::endl;
    std::cout << "ubx: max = " << ui->ubxMax->value() << " min = " << ui->ubxMin->value() << std::endl << std::endl;

    msg.disable_collision_avoidance.data = !ui->collision_avoidance_->isChecked();
    msg.robot_collision_padding.data = ui->padding_->value();
    msg.trajectory_time_factor.data = ui->time_factor_->value();
    msg.octomap_max_height.data = ui->octomap_height_->value();
    msg.goal_cube_clearance.data = ui->octomap_clearance_cube_dimensions_->value();


    constraints_pub_.publish(msg);
}

void joint_limit::timerEvent(QTimerEvent *event)
{
	// check if ros is still running; if not, just kill the application
    if(!ros::ok())
        qApp->quit();
        
    //Spin at beginning of Qt timer callback, so current ROS time is retrieved
    ros::spinOnce();
}

void joint_limit::on_Presets_comboBox_currentIndexChanged(int index)
{
    switch(index){
    case 0:
        lbzMinVal = -0.610865;
        lbzMaxVal = 0.610865;
        mbyMinVal = -1.2;
        mbyMaxVal = 1.28;
        ubxMinVal = -0.790809;
        ubxMaxVal = 0.790809;
        break;
    case 1:
        lbzMinVal = 0.0;
        lbzMaxVal = 0.0;
        mbyMinVal = 0.0;
        mbyMaxVal = 0.0;
        ubxMinVal = 0.0;
        ubxMaxVal = 0.0;
        break;
    case 2:
        lbzMinVal = -0.610865;
        lbzMaxVal = 0.610865;
        mbyMinVal = 0.0;
        mbyMaxVal = 0.28;
        ubxMinVal = -0.2;
        ubxMaxVal = 0.2;
        break;
    default: break;
    }

    ui->lbzMin->setValue(lbzMinVal*1000000.0);
    ui->lbzMinLabel->setText(QString::number(lbzMinVal,'g',6));
    ui->lbzMax->setValue(lbzMaxVal*1000000.0);
    ui->lbzMaxLabel->setText(QString::number(lbzMaxVal,'g',6));

    ui->mbyMin->setValue(mbyMinVal*100.0);
    ui->mbyMinLabel->setText(QString::number(mbyMinVal,'g',6));
    ui->mbyMax->setValue(mbyMaxVal*100.0);
    ui->mbyMaxLabel->setText(QString::number(mbyMaxVal,'g',6));

    ui->ubxMin->setValue(ubxMinVal*1000000.0);
    ui->ubxMinLabel->setText(QString::number(ubxMinVal,'g',6));
    ui->ubxMax->setValue(ubxMaxVal*1000000.0);
    ui->ubxMaxLabel->setText(QString::number(ubxMaxVal,'g',6));
}

void joint_limit::processNewKeyEvent(const flor_ocs_msgs::OCSKeyEvent::ConstPtr &key_event)
{
    // store key state
    if(key_event->state)
        keys_pressed_list_.push_back(key_event->key);
    else
        keys_pressed_list_.erase(std::remove(keys_pressed_list_.begin(), keys_pressed_list_.end(), key_event->key), keys_pressed_list_.end());

    // process hotkeys
    std::vector<int>::iterator key_is_pressed;

    key_is_pressed = std::find(keys_pressed_list_.begin(), keys_pressed_list_.end(), 37);
    if(key_event->key == 13 && key_event->state && key_is_pressed != keys_pressed_list_.end()) // ctrl+4
    {
        if(this->isVisible())
        {
            this->hide();
        }
        else
        {
            this->move(QPoint(key_event->cursor_x+5, key_event->cursor_y+5));
            this->show();
        }
    }
}

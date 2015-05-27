#include "glancehubSbar.h"
#include <ros/ros.h>
#include "ui_glancehubSbar.h"
#include <ros/package.h>
#include <QStandardItemModel>

glancehubSbar::glancehubSbar(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::glancehubSbar)
{
    ui->setupUi(this);
    ghub_ = new glancehub(this);
    //remove window border and set on top
    Qt::WindowFlags flags = ghub_->windowFlags();
    flags |= Qt::WindowStaysOnTopHint;
    flags |= Qt::FramelessWindowHint;
    flags |= Qt::Dialog; // //ensure ghub_ as a dialog box, not a seperate window/tab
    ghub_->setWindowFlags(flags);

    ghub_->setWindowOpacity(0);

    // load control modes into dropdown box from parameters
    nh_.getParam("/atlas_controller/allowed_control_modes", allowed_control_modes_);
    ROS_INFO(" Add %ld allowable control modes:", allowed_control_modes_.size());
    for(int i = 0; i < allowed_control_modes_.size(); i++)
    {
        std::cout << allowed_control_modes_[i] << std::endl;
        ui->modeBox->addItem(allowed_control_modes_[i].c_str());
    }

    ui->modelabel->setText(""); // default setting is off on start
    previous_selection_ = "none";

    //sets first item to unselectable
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->modeBox->model());
    QModelIndex firstIndex = model->index(0, ui->modeBox->modelColumn(), ui->modeBox->rootModelIndex());
    QStandardItem* firstItem = model->itemFromIndex(firstIndex);
    if(firstItem != NULL) firstItem->setSelectable(false);

    // Now connect signals
    connect(ghub_,SIGNAL(sendMoveitStatus(bool)),this,SLOT(receiveMoveitStatus(bool)));
    connect(ghub_,SIGNAL(sendFootstepStatus(int)),this,SLOT(receiveFootstepStatus(int)));
    connect(ghub_,SIGNAL(sendFlorStatus(int)),this,SLOT(receiveFlorStatus(int)));
    connect(ui->modeBox,SIGNAL(currentIndexChanged(int)),this,SLOT(modeChanged(int)));

    //set popup width larger
    //ui->modeBox->view()->setFixedWidth(30);

    //setup publisher to change modes
    mode_pub_ = nh_.advertise<flor_control_msgs::FlorControlModeCommand>("/flor/controller/mode_command", 5, false);

    ui->plannerLight->setStyleSheet("QLabel { background-color: white; border:2px solid grey; }");
    ui->footstepLight->setStyleSheet("QLabel { background-color: white; border:2px solid grey; }");

    //using down arrow from map view TODO: move icons to seperate directory
    std::string ip = ros::package::getPath("vigir_ocs_map_view")+"/icons/";
    QString icon_path = QString(ip.c_str());
    // workaround to be able to use images from stylesheet without knowing the path in advance
    QString stylesheet = ui->modeBox->styleSheet() + "\n" +
            "QComboBox::down-arrow {\n" +
            " image: url(" + icon_path + "down_arrow.png" + ");\n" +
            "}";
    ui->modeBox->setStyleSheet(stylesheet);


    ui->plannerLight->setToolTip("waiting for status update");
    ui->moveitLabel->setToolTip("waiting for status update");

    ui->footstepLight->setToolTip("waiting for status update");
    ui->footstepLabel->setToolTip("waiting for status update");

    color_timer_.start(300,this);
    max_flashes_ = 6;
    flashing_move_it_ = false;
    flashing_footstep_ = false;
    colored_moveit_ = false;
    colored_footstep_ = false;
    ignore_events_ = false;
    flash_footstep_counter_ = 0;
    flash_moveit_counter_ = 0;
    white_ = "QLabel {background-color: white; border:2px solid grey;}";
}

glancehubSbar::~glancehubSbar()
{
    delete ghub_;
    delete ui;
}

void glancehubSbar::timerEvent(QTimerEvent *event)
{
    if(flashing_move_it_)
    {
        if(flash_moveit_counter_ < max_flashes_)
        {
            //flash moveit light
            if(colored_moveit_)
            {
                ui->plannerLight->setStyleSheet(white_);
                colored_moveit_ = !colored_moveit_;
            }
            else
            {
                ui->plannerLight->setStyleSheet(flash_color_moveit_);
                colored_moveit_ = !colored_moveit_;
                flash_moveit_counter_++;
            }
        }
        else
        {
            //flashing done. reset and wait for next call
            //counter is reset by functions that accepts new states
            flashing_move_it_ = false;
        }
    }

    if(flashing_footstep_)
    {
        if(flash_footstep_counter_ < max_flashes_)
        {
            //flash footstep light
            if(colored_footstep_)
            {
                ui->footstepLight->setStyleSheet(white_);
                colored_footstep_ = !colored_footstep_;
            }
            else
            {
                ui->footstepLight->setStyleSheet(flash_color_footstep_);
                colored_footstep_ = !colored_footstep_;
                flash_footstep_counter_++;
            }
        }
        else
        {
            flashing_footstep_ = false;
        }
    }
}

void glancehubSbar::modeChanged(int mode)
{
    if(ignore_events_)
        return;

    ui->modelabel->setStyleSheet("QLabel{color:red; }");

    QString newText;
    if (mode >= 0 && mode <  allowed_control_modes_.size())
        newText = QString::fromStdString(allowed_control_modes_[mode]);
    else
        newText = QString::fromStdString("Unknown");

    ui->modelabel->setText(previous_selection_+" -> "+newText);

    // only publish the mode selection command if this slot is called from UI or if publish is not set to false
    flor_control_msgs::FlorControlModeCommand msg;
    msg.header.stamp = ros::Time::now();
    msg.requested_control_mode = mode;
    previous_selection_ = newText;
    mode_pub_.publish(msg);
}

void glancehubSbar::receiveMoveitStatus(bool status)
{
    if(status)
    {
        //moveit failed
        flash_color_moveit_ = "QLabel { background-color: red;border:2px solid grey; }";

        //notify ui on failure
        NotificationSystem::Instance()->notifyWarning("Moveit Failed");
    }
    else
    {
        //moveit success
       flash_color_moveit_ = "QLabel { background-color: green; border:2px solid grey; }";

       // notify ui on success
       NotificationSystem::Instance()->notifyPassive("Moveit Success");
    }
    ui->plannerLight->setToolTip(ghub_->getMoveitStat());
    ui->moveitLabel->setToolTip(ghub_->getMoveitStat());
    flashing_move_it_ = true;
    flash_moveit_counter_ = 0; // reset counter here because we want to flash latest color 10 times (this function may be called multiple times in short span)

}

void glancehubSbar::receiveFootstepStatus(int status)
{
    switch(status)
    {
    case flor_ocs_msgs::OCSFootstepStatus::FOOTSTEP_PLANNER_ACTIVE: case flor_ocs_msgs::OCSFootstepStatus::FOOTSTEP_EXECUTION_ACTIVE:
        NotificationSystem::Instance()->notifyPassive("Footstep Planner Active");
        flash_color_footstep_ = "QLabel { background-color: yellow; border:2px solid grey; }";
        break;
    case flor_ocs_msgs::OCSFootstepStatus::FOOTSTEP_PLANNER_SUCCESS: case flor_ocs_msgs::OCSFootstepStatus::FOOTSTEP_VALID_GOAL: //case flor_ocs_msgs::OCSFootstepStatus::FOOTSTEP_EXECUTION_SUCCESS: //same as FOOTSTEP_PLANNER_SUCCESS
        NotificationSystem::Instance()->notifyPassive("Footstep Planner Succeded");
        flash_color_footstep_ = "QLabel { background-color: green; border:2px solid grey; }";
        break;
    default: // all the error codes go here
        NotificationSystem::Instance()->notifyError("Footstep Planner Failed");
        flash_color_footstep_ = "QLabel { background-color: red; border:2px solid grey; }";
        break;
    }
    //notify ui on failure


    ui->footstepLight->setToolTip(ghub_->getFootstepStat());
    ui->footstepLabel->setToolTip(ghub_->getFootstepStat());
    flashing_footstep_ = true;
    flash_footstep_counter_ = 0;
}

void glancehubSbar::receiveFlorStatus(int status)
{
    // do not set status if it didn't change
    if(ui->modeBox->currentIndex() != status && status >= 0 && status <  allowed_control_modes_.size())
    {
        ignore_events_ = true;

        // since we're already receiving an UI event for this, set current index but use event filter to ignore events
        ui->modeBox->setCurrentIndex(status);

        //notify on 3d view  previous is still at the state we want right now
        QString notification_text = QString("Changed Robot Mode to ") + ui->modeBox->currentText();// + previous_selection_ ;
        //flor stop is an error condition
        //if(previous_selection_ == "stop")
        if(ui->modeBox->currentText() == "stop")
            NotificationSystem::Instance()->notifyError(notification_text.toStdString());
        else if(ui->modeBox->currentText() == "")
            NotificationSystem::Instance()->notifyWarning("Invalid Mode Change");
        else
            NotificationSystem::Instance()->notifyPassive(notification_text.toStdString());

        ignore_events_ = false;
    }
    ui->modelabel->setText(""); // only want to display transitions    
}

bool glancehubSbar::eventFilter( QObject * o, QEvent * e )
{
    e->ignore();
    return false;
}

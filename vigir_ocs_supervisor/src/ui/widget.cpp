#include "widget.h"
#include "ui_widget.h"
#include<string.h>
# include <QAbstractButton>
#include<QLabel>
#include<ros/ros.h>
#include <ros/package.h>
#include<QHBoxLayout>
#include<QGridLayout>
#include<QTableWidgetItem>


Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    bold.setBold(true);
    normal.setBold(false);
//    ui->stat->setColumnCount(3);
    maxRows = 100;
    avg_inlet_pr = -1;
    avg_air_sump_pressure= -1;
    avg_pump_rpm=-1;
    avg_pump_return_pressure=-1;
    avg_pump_supply_pressure=-1;
    ui->setupUi(this);
    ui->cs->setEnabled(false);
    ui->cs_list->setEnabled(false);
    ui->cur_st->setEnabled(false);
    //ui->getlog->setEnabled(false);
    ui->high->setEnabled(false);
    ui->last_stat->setEnabled(false);
    ui->pr->setEnabled(false);
    ui->curst->setEnabled(false);
//    ui->stat->setEnabled(false);
    ui->robo_st->setEnabled(false);
    ui->send_mode->setEnabled(false);
    ui->start->setEnabled(false);
    ui->off->setEnabled(false);
    ui->low->setEnabled(false);
    ui->connect->setStyleSheet("background-color: grelabel_2en; color: black");
    ui->d_state->setEnabled(false);
    ui->d_label->setEnabled(false);
    ui->r_state->setEnabled(false);
    ui->pinlet->setEnabled(false);
    ui->psump->setEnabled(false);
    ui->psupply->setEnabled(false);
    ui->preturn->setEnabled(false);
    ui->return_2->setEnabled(false);
    ui->sump->setEnabled(false);
    ui->supply->setEnabled(false);
    ui->inlet->setEnabled(false);
    ui->rpm->setEnabled(false);
    ui->prpm->setEnabled(false);
    ui->timemeter->setEnabled(false);
    ui->ptimemeter->setEnabled(false);
    ui->rfault->setEnabled(false);
    ui->fault->setEnabled(false);
    ui->pst->setEnabled(false);
    ui->ppst->setEnabled(false);
    ui->mt->setEnabled(false);
    ui->mdt->setEnabled(false);
    ui->pmdt->setEnabled(false);
    ui->pmt->setEnabled(false);
    //sub_control = nh.subscribe<flor_control_msgs::FlorRobotStateCommand>("/flor/controller/robot_state_command", 5, &Widget::controlstate, this);
    pub = nh.advertise<flor_control_msgs::FlorRobotStateCommand> ("/flor/controller/robot_state_command",5,false);
    sub_state = nh.subscribe<flor_control_msgs::FlorRobotStatus>("/flor/controller/robot_status", 5, &Widget::robotstate, this);
    sub_behav = nh.subscribe<atlas_msgs::AtlasSimInterfaceState>("/atlas/atlas_sim_interface_state", 5, &Widget::behavstate, this);
    sub_fault = nh.subscribe<flor_control_msgs::FlorRobotFault >("/flor/controller/robot_fault", 5, &Widget::robotfault, this);
    //changed here
//    status_msg_sub = nh.subscribe<flor_ocs_msgs::OCSRobotStatus>( "/flor_robot_status", 100, &Widget::recievedMessage, this );
    timer.start(1, this);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::timerEvent(QTimerEvent *event)
{
    //Spin at beginning of Qt timer callback, so current ROS time is retrieved
    ros::spinOnce();
}


void Widget::on_connect_clicked()
{
    if (ui->connect->text()=="CONNECT")
    {ui->connect->setText("CONNECT");
        ui->connect->setStyleSheet("background-color: green; color: black");
        ui->start->setStyleSheet("background-color: gray; color: black");
        ui->start->setEnabled(false);

        //publishing command "CONNECT"
        flor_control_msgs::FlorRobotStateCommand connect ;
        connect.state_command=flor_control_msgs::FlorRobotStateCommand::CONNECT;
        pub.publish(connect);
    }
    else
    {
        /*ui->connect->setText("CONNECT");
        ui->connect->setStyleSheet("background-color: green; color: black");
        ui->start->setStyleSheet("background-color: gray; color: black");
        ui->start->setEnabled(false);*/
        //publishing command "DISCONNECT"last_run_state
        flor_control_msgs::FlorRobotStateCommand disconnect ;
        disconnect.state_command=flor_control_msgs::FlorRobotStateCommand::DISCONNECT;
        pub.publish(disconnect);
    }
}

void Widget::recievedMessage(const flor_ocs_msgs::OCSRobotStatus::ConstPtr& msg)
{
    uint8_t  level;
       uint16_t code;
       RobotStatusCodes::codes(msg->status, code,level); //const uint8_t& error, uint8_t& code, uint8_t& severity)
       //std::cout << "Recieved message. level = " << (int)level << " code = " << (int)code << std::endl;
       QTableWidgetItem* text = new QTableWidgetItem();
       QTableWidgetItem* msgType = new QTableWidgetItem();
       QTableWidgetItem* time = new QTableWidgetItem();
       time->setText(timeFromMsg(msg->stamp));
       text->setFlags(text->flags() ^ Qt::ItemIsEditable);
       time->setFlags(time->flags() ^ Qt::ItemIsEditable);
       msgType->setFlags(msgType->flags() ^ Qt::ItemIsEditable);
       switch(level){
       case 0:
           msgType->setText("Ok");
           break;
       case 1:
           msgType->setText("Debug");
           break;
       case 2:
           msgType->setText("Warn");
           text->setBackgroundColor(Qt::yellow);
           time->setBackgroundColor(Qt::yellow);
           msgType->setBackgroundColor(Qt::yellow);
           numWarn++;
           break;
       case 3:
           msgType->setText("Error");
           text->setBackgroundColor(Qt::red);
           time->setBackgroundColor(Qt::red);
           msgType->setBackgroundColor(Qt::red);
           numError++;
       }

       if(code >= errors.size() && errors.size() != 0)
       {
           std::cout << "Recieved message (Default Message). level = " << (int)level << " code = " << (int)code << std::endl;
           QString tempMessage = QString::fromStdString("Default Message");
           tempMessage+=QString::number(code);
           text->setText(tempMessage);
           text->setBackgroundColor(Qt::red);
           time->setBackgroundColor(Qt::red);
           msgType->setBackgroundColor(Qt::red);
           numError++;
       }
       else if(errors.size() > 0)
           text->setText(QString::fromStdString(errors[code]));
       else
       {
           std::cout << "Cannot find data file but recieved msg level = " << (int)level << " code = " << (int)code << std::endl;

           QString tempMessage = "Cannot find data file but recieved msg  num";
           tempMessage+= QString::number(code);
           text->setText(tempMessage);
           text->setBackground(Qt::red);
           msgType->setBackgroundColor(Qt::red);
           time->setBackgroundColor(Qt::red);
           numError++;
       }

       msgType->setFont(bold);
       time->setFont(bold);
       text->setFont(bold);
       std::vector<completeRow*>::iterator it;
       it = messages.begin();

       messages.insert(it,new completeRow());
       messages[0]->time = time;
       messages[0]->priority = msgType;
       messages[0]->text = text;
       ui->stat->insertRow(0);
       //std::cout << "Adding item to table... " << messages.size() <<  " " << messages[0]->text << std::endl;
       ui->stat->setItem(0,0,messages[0]->time);
       ui->stat->setItem(0,1,messages[0]->priority);
       ui->stat->setItem(0,2,messages[0]->text);
       for(int i=0;i<5;i++)
           ui->stat->showRow(i);

      /* if(messages[0]->priority->text() == "Ok" && showOk->isChecked())
       {
           msgTable->showRow(0);
       }
       else if(messages[0]->priority->text() == "Debug" && showDebug->isChecked())
       {
           msgTable->showRow(0);
       }
       else if(messages[0]->priority->text() == "Warn" && showWarn->isChecked())
       {
           msgTable->showRow(0);
       }
       else if(messages[0]->priority->text() == "Error" && showError->isChecked())
       {
           msgTable->showRow(0);
       }
       else
           msgTable->hideRow(0);
           */
       //std::cout << "Item added sucessfuly..." << std::endl;
       if(messages.size() > maxRows)
       {
           if(messages[messages.size()-1]->priority->text() == "Warn")
               numWarn--;
           else if(messages[messages.size()-1]->priority->text() == "Error")
               numError--;
           messages.pop_back();
           ui->stat->removeRow(maxRows);
       }
       unreadMsgs++;

}
QString Widget::timeFromMsg(ros::Time stamp)
{

    double dSec = stamp.toSec();
    int sec = dSec;
    std::stringstream stream;

    stream.str("");
    int day = sec/86400;
    sec -= day * 86400;

    int hour = sec / 3600;
    sec -= hour * 3600;

    int min = sec / 60;
    sec -= min * 60;

    int iSec = dSec;
    dSec -= iSec;
    int ms = (dSec*1000.0);

    stream << std::setw(2) << std::setfill('0') << day << " ";
    stream << std::setw(2) << std::setfill('0') << hour << ":";
    stream << std::setw(2) << std::setfill('0') << min << ":";
    stream << std::setw(2) << std::setfill('0') << sec << ".";
    stream << std::setw(3) << std::setfill('0') << ms ;
    return QString::fromStdString(stream.str());
}
 void Widget:: robotstate( const flor_control_msgs::FlorRobotStatus::ConstPtr& msg )
 {
     // save the last status message
     last_run_state = msg->robot_run_state;
     switch(msg->robot_run_state)
     {
     case 0:ui->r_state->setText("IDLE");break;
     case 1:ui->r_state->setText("START");break;
     case 3:ui->r_state->setText("CONTROL");break;
     case 5:ui->r_state->setText("STOP");break;
     }

     // Initialize the averages on first pass
     if(avg_inlet_pr==-1)
     {
         avg_inlet_pr                = msg->pump_inlet_pressure;
         avg_air_sump_pressure       = msg->air_sump_pressure;
         avg_pump_rpm                = msg->current_pump_rpm;
         avg_pump_return_pressure    = msg->pump_return_pressure;
         avg_pump_supply_pressure    = msg->pump_supply_pressure;
         avg_pump_supply_temperature = msg->pump_supply_temperature;
         avg_motor_temperature       = msg->motor_temperature;
         avg_motor_driver_temp       = msg->motor_driver_temperature;
     }


     // Average the noisy signals
     avg_inlet_pr               = 0.1*msg->pump_inlet_pressure      + 0.9*avg_inlet_pr            ;
     avg_air_sump_pressure      = 0.1*msg->air_sump_pressure        + 0.9*avg_air_sump_pressure   ;
     avg_pump_rpm               = 0.1*msg->current_pump_rpm         + 0.9*avg_pump_rpm            ;
     avg_pump_return_pressure   = 0.1*msg->pump_return_pressure     + 0.9*avg_pump_return_pressure;
     avg_pump_supply_pressure   = 0.1*msg->pump_supply_pressure     + 0.9*avg_pump_supply_pressure;
     avg_pump_supply_temperature= 0.1*msg->pump_supply_temperature  + 0.9*avg_pump_supply_temperature;
     avg_motor_temperature      = 0.1*msg->motor_temperature        + 0.9*avg_motor_temperature      ;
     avg_motor_driver_temp      = 0.1*msg->motor_driver_temperature + 0.9*avg_motor_driver_temp      ;

     // Update the text
     ui->inlet->setText(QString::number(avg_inlet_pr,'f',2));
     ui->sump->setText(QString::number(avg_air_sump_pressure,'f',2));
     ui->rpm->setText(QString::number(avg_pump_rpm,'f',2));
     ui->return_2->setText(QString::number(avg_pump_return_pressure,'f',2));
     ui->supply->setText(QString::number(avg_pump_supply_pressure,'f',2));
     ui->pst->setText(QString::number(avg_pump_supply_temperature,'f',2));
     ui->mt->setText(QString::number(avg_motor_temperature,'f',2));
     ui->mdt->setText(QString::number(avg_motor_driver_temp,'f',2));

     // Just set the time meter (no averaging)
     ui->timemeter->setText(QString::number(msg->pump_time_meter,'f',2));


     // Set the alarm colors on raw values
     if (msg->pump_supply_temperature>94.0)
         ui->pst->setStyleSheet("background-color: red");
     else if ((89.0 < msg->pump_supply_temperature) && (msg->pump_supply_temperature <= 94.0))
         ui->pst->setStyleSheet("background-color: yellow");
     else
         ui->pst->setStyleSheet("background-color: white");

     if (msg->motor_temperature>149.0)
         ui->mt->setStyleSheet("background-color: red");
     else if((124.0<msg->motor_temperature) && (msg->motor_temperature <=149.0))
         ui->mt->setStyleSheet("background-color: yellow");
     else
         ui->mt->setStyleSheet("background-color: white");

     if (msg->motor_driver_temperature>59.0)
         ui->mdt->setStyleSheet("background-color: red");
     else if ((54.0 < msg->motor_driver_temperature) && (msg->motor_driver_temperature<59.0))
         ui->mdt->setStyleSheet("background-color: yellow");
     else
         ui->mdt->setStyleSheet("background-color: white");

     // code to detect fault in inlet pressure
     if (msg->pump_inlet_pressure<50.0)
         ui->inlet->setStyleSheet("background-color: red");
     else if((50.0<msg->pump_inlet_pressure) && (msg->pump_inlet_pressure <70.0))
         ui->inlet->setStyleSheet("background-color: yellow");
     else
         ui->inlet->setStyleSheet("background-color: white");

     if (avg_air_sump_pressure<50.0)
         ui->sump->setStyleSheet("background-color: red");
     else if((50.0 <avg_air_sump_pressure) && (avg_air_sump_pressure<70.0))
         ui->sump->setStyleSheet("background-color: yellow");
     else
         ui->sump->setStyleSheet("background-color: white");

     if (msg->pump_return_pressure<50)
         ui->return_2->setStyleSheet("background-color: red");
     else if((50.0 <msg->pump_return_pressure) && (msg->pump_return_pressure<70.0))
         ui->return_2->setStyleSheet("background-color: yellow");
     else
         ui->return_2->setStyleSheet("background-color: white");


     if (msg->pump_supply_pressure<1500.0)
         ui->supply->setStyleSheet("background-color: red");
     else if ((1500.0 <msg->pump_supply_pressure) && (msg->pump_supply_pressure<2700.0))
         ui->supply->setStyleSheet("background-color: yellow");
     else
         ui->supply->setStyleSheet("background-color: white");

     // check if we are connected to the robotfalse
     if(msg->robot_connected==1)
     {
         ui->connect->setText("DISCONNECT");
         ui->connect->setStyleSheet("background-color: red; color: black");

         ui->pr->setEnabled(true);
         ui->high->setEnabled(true);
         ui->off->setEnabled(true);
         ui->low->setEnabled(true);
     }
     if(msg->robot_connected ==0)
     {
         ui->connect->setText("CONNECT");
         ui->connect->setStyleSheet("background-color: green; color: black");
         ui->start->setStyleSheet("background-color: gray; color: black");
         ui->start->setEnabled(false);
     }
     // check if we need to enable start
     if(msg->robot_run_state==0)
       enableStart();

     // check if run_state is different than idle to enable stop and all the other options in the UI
     if(msg->robot_run_state!=0)
     {
         ui->start->setEnabled(true);
         ui->start->setText("STOP");
         ui->start->setStyleSheet("background-color: red; color: black");
         ui->cs->setEnabled(true);
         ui->cs_list->setEnabled(true);
         ui->cur_st->setEnabled(true);
         ui->last_stat->setEnabled(true);
         ui->curst->setEnabled(true);
         ui->stat->setEnabled(true);
         ui->robo_st->setEnabled(true);
         ui->d_label->setEnabled(true);
         ui->d_state->setEnabled(true);
         ui->r_state->setEnabled(true);
         ui->send_mode->setEnabled(true);
         ui->pinlet->setEnabled(true);
         ui->psump->setEnabled(true);
         ui->psupply->setEnabled(true);
         ui->preturn->setEnabled(true);
         ui->pr->setEnabled(false);
         ui->off->setEnabled(false);
         ui->low->setEnabled(false);
         ui->high->setEnabled(false);
         ui->rpm->setEnabled(true);
         ui->prpm->setEnabled(true);
         ui->timemeter->setEnabled(true);
         ui->ptimemeter->setEnabled(true);
         ui->rfault->setEnabled(true);
         ui->fault->setEnabled(true);
         ui->pst->setEnabled(true);
         ui->ppst->setEnabled(true);
         ui->mt->setEnabled(true);
         ui->mdt->setEnabled(true);
         ui->pmdt->setEnabled(true);
         ui->pmt->setEnabled(true);
     }

     if(msg->robot_critical_fault==1)
     {

         QLabel *fault_label = new QLabel();
         fault_label->setStyleSheet("background-color:yellow");
         QHBoxLayout *h = new QHBoxLayout();
         h->addWidget(fault_label);
         h->addWidget(ui->connect);
         ui->widget->setLayout(h);


     }

 }
 void Widget::robotfault(const flor_control_msgs::FlorRobotFault::ConstPtr& msg)
 {

     ui->fault->setText(msg->message.c_str());

 }

void Widget:: behavstate( const atlas_msgs::AtlasSimInterfaceState::ConstPtr& msg )
{
    switch(msg->current_behavior)
    {
    case 0: ui->cur_st->setText("STAND"); break;
    case 1: ui->cur_st->setText("USER"); break;
    case 2: ui->cur_st->setText("FREEZE"); break;
    case 3: ui->cur_st->setText("STAND_PREP"); break;
    case 4: ui->cur_st->setText("WALK"); break;
    case 5: ui->cur_st->setText("STEP"); break;
    case 6: ui->cur_st->setText("MANIPULATE"); break;
    }
    switch(msg->desired_behavior)
    {
    case 0: ui->d_state->setText("STAND"); break;
    case 1: ui->d_state->setText("USER"); break;
    case 2: ui->d_state->setText("FREEZE"); break;
    case 3: ui->d_state->setText("STAND_PREP"); break;
    case 4: ui->d_state->setText("WALK"); break;
    case 5: ui->d_state->setText("STEP"); break;
    case 6: ui->d_state->setText("MANIPULATE"); break;
    }

}
void Widget:: controlstate(const flor_control_msgs::FlorRobotStateCommand::ConstPtr& msg)
{
    if(msg->state_command==flor_control_msgs::FlorRobotStateCommand::FREEZE)
        ui->cs_list->setCurrentItem(ui->cs_list->findItems("FREEZE",Qt::MatchExactly)[0]);
    else if(msg->state_command==flor_control_msgs::FlorRobotStateCommand::STAND)
        ui->cs_list->setCurrentItem(ui->cs_list->findItems("STAND",Qt::MatchExactly)[0]);
    else if(msg->state_command==flor_control_msgs::FlorRobotStateCommand::STAND_PREP)
        ui->cs_list->setCurrentItem(ui->cs_list->findItems("STAND PREP",Qt::MatchExactly)[0]);

}
void Widget::on_start_clicked()
{
    if(ui->off->isChecked())
    {
        flor_control_msgs::FlorRobotStateCommand off ;
        off.state_command = flor_control_msgs::FlorRobotStateCommand::START_HYDRAULIC_PRESSURE_OFF;
        pub.publish(off);
    }
    else if(ui->low->isChecked())
    {
        flor_control_msgs::FlorRobotStateCommand low ;
        low.state_command = flor_control_msgs::FlorRobotStateCommand::START_HYDRAULIC_PRESSURE_LOW;
        pub.publish(low);
    }
    else if(ui->high->isChecked())
    {
        flor_control_msgs::FlorRobotStateCommand high ;
        high.state_command = flor_control_msgs::FlorRobotStateCommand::START_HYDRAULIC_PRESSURE_HIGH;
        pub.publish(high);
    }

    if (ui->start->text()=="STOP")
    {
        ui->start->setText("START");
        ui->start->setEnabled(false);
        ui->start->setStyleSheet("background-color: gray; color: black");
        ui->cs->setEnabled(false);
        ui->cs_list->setEnabled(false);
        ui->cur_st->setEnabled(false);
        ui->last_stat->setEnabled(false);
        ui->curst->setEnabled(false);
        ui->stat->setEnabled(false);
        ui->robo_st->setEnabled(false);
        ui->d_label->setEnabled(false);
        ui->d_state->setEnabled(false);
        ui->r_state->setEnabled(false);
        ui->send_mode->setEnabled(false);
        //ui->start->setEnabled(true);
        ui->pinlet->setEnabled(false);
        ui->psump->setEnabled(false);
        ui->psupply->setEnabled(false);
        ui->preturn->setEnabled(false);
        ui->pr->setEnabled(false);
        ui->off->setEnabled(false);
        ui->low->setEnabled(false);
        ui->high->setEnabled(false);
        ui->pr->setEnabled(false);
        ui->high->setEnabled(false);
        ui->off->setEnabled(false);
        ui->low->setEnabled(false);
        ui->rpm->setEnabled(false);
        ui->prpm->setEnabled(false);
        ui->timemeter->setEnabled(false);
        ui->ptimemeter->setEnabled(false);
        ui->rfault->setEnabled(false);
        ui->fault->setEnabled(false);
        // NEED TO SEND STOP MESSAGE HERE
        flor_control_msgs::FlorRobotStateCommand stop ;
        stop.state_command = flor_control_msgs::FlorRobotStateCommand::STOP;
        pub.publish(stop);
    }
}

// Checks if start should be enabled
void Widget::enableStart()
{
    if(last_run_state==0 && (ui->low->isChecked() || ui->off->isChecked() || ui->high->isChecked()))
    {
        ui->start->setEnabled(true);
        ui->start->setText("START");
        ui->start->setStyleSheet("background-color: green; color: black");
        //ui->pr->setEnabled(false);
        //ui->high->setEnabled(false);
        //ui->off->setEnabled(false);
        //ui->low->setEnabled(false);
    }
}

void Widget::on_off_clicked()
{
    enableStart();
}

void Widget::on_low_clicked()
{
    enableStart();
}

void Widget::on_high_clicked()
{
    enableStart();
}

void Widget::on_send_mode_clicked()
{
    if (ui->cs_list->currentItem()->text()=="FREEZE")
    {
        flor_control_msgs::FlorRobotStateCommand freeze ;
       freeze.state_command  = flor_control_msgs::FlorRobotStateCommand::FREEZE;
        pub.publish(freeze);
   }

   if (ui->cs_list->currentItem()->text()=="STAND")
   {
        flor_control_msgs::FlorRobotStateCommand stand ;
       stand.state_command  = flor_control_msgs::FlorRobotStateCommand::STAND;
        pub.publish(stand);
   }


    if (ui->cs_list->currentItem()->text()=="STAND PREP")
    {
        flor_control_msgs::FlorRobotStateCommand stand_prep ;
       stand_prep.state_command= flor_control_msgs::FlorRobotStateCommand::STAND_PREP;
        pub.publish(stand_prep);
    }
}

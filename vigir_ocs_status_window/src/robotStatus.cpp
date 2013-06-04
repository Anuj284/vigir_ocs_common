#include "robotStatus.h"
#include <QVBoxLayout>
#include <ros/ros.h>
#include <flor_ocs_msgs/RobotStatusCodes.h>

robotStatus::robotStatus(QWidget *parent) :
    QWidget(parent)
{
    bold.setBold(true);
    normal.setBold(false);
    msgTable = new QTableWidget();
    msgTable->setColumnCount(3);
    clearButton = new QPushButton();
    clearButton->setText("Clear Table Contents");

    showOk = new QCheckBox();
    showWarn = new QCheckBox();
    showDebug = new QCheckBox();
    showError = new QCheckBox();

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(msgTable);
    main_layout->addWidget(clearButton);
    main_layout->addWidget(showOk);
    main_layout->addWidget(showDebug);
    main_layout->addWidget(showWarn);
    main_layout->addWidget(showError);
    showOk->setText("Show Ok channel");
    showOk->click();
    showDebug->setText("Show Debug Channel");
    showDebug->click();
    showWarn->setText("Show Warn Channel");
    showWarn->click();
    showError->setText("Show Error Channel");
    showError->click();

    //std::cout << "Adding layout..." << std::endl;
    setLayout(main_layout);
    this->setMinimumSize(550,400);
    msgTable->setColumnWidth(0,140);
    msgTable->setColumnWidth(1,50);
    msgTable->setColumnWidth(2,315);
    //labels = new QStringList();
    labels.push_back("Sim Time");
    labels.push_back("Type");
    labels.push_back("Message Contents");
    msgTable->setHorizontalHeaderLabels(labels);
    unreadMsgs=0;
    numError = 0;
    numWarn = 0;
    ros::NodeHandle nh;
    std::string fileName;
    if(nh.getParam("robotErrorFileLocation",fileName))
            messagesFile.setFileName(fileName.c_str());
    else
            messagesFile.setFileName("/home/messages.csv");
    loadFile();
    rosSubscriber = nh.subscribe<flor_ocs_msgs::OCSRobotStatus>( "/robot_status", 2, &robotStatus::recievedMessage, this );
    std::cout << "Done setting up waiting for messages." << std::endl;
    ros::spinOnce();
    clearButton->connect(clearButton,SIGNAL(clicked()),this,SLOT(on_clearButton_clicked()));
    msgTable->connect(msgTable,SIGNAL(cellClicked(int,int)),this,SLOT(on_msgTable_cellClicked(int,int)));
    showOk->connect(showOk,SIGNAL(clicked()),this,SLOT(on_radioButtons_updated()));
    showDebug->connect(showDebug,SIGNAL(clicked()),this,SLOT(on_radioButtons_updated()));
    showWarn->connect(showWarn,SIGNAL(clicked()),this,SLOT(on_radioButtons_updated()));
    showError->connect(showError,SIGNAL(clicked()),this,SLOT(on_radioButtons_updated()));

    timer.start(33, this);
}

void robotStatus::timerEvent(QTimerEvent *event)
{
    //Spin at beginning of Qt timer callback, so current ROS time is retrieved
    ros::spinOnce();
}

void robotStatus::on_radioButtons_updated()
{
    std::cout <<"Radio buttons changed updating list..." <<std::endl;
    for(int i=0;i<messages.size();i++)
    {
        if(messages[i]->priority->text() == "Ok" && showOk->isChecked())
        {
            msgTable->showRow(i);
        }
        else if(messages[i]->priority->text() == "Debug" && showDebug->isChecked())
        {
            msgTable->showRow(i);
        }
        else if(messages[i]->priority->text() == "Warn" && showWarn->isChecked())
        {
            msgTable->showRow(i);
        }
        else if(messages[i]->priority->text() == "Error" && showError->isChecked())
        {
            msgTable->showRow(i);
        }
        else
            msgTable->hideRow(i);
    }
}

QString robotStatus::timeFromMsg(const ros::Time stamp)
{
    int sec = stamp.toSec();
    std::stringstream stream;

    stream.str("");
    int day = sec/86400;
    sec -= day * 86400;

    int hour = sec / 3600;
    sec -= hour * 3600;

    int min = sec / 60;
    sec -= min * 60;

    stream << std::setw(2) << std::setfill('0') << day << " ";
    stream << std::setw(2) << std::setfill('0') << hour << ":";
    stream << std::setw(2) << std::setfill('0') << min << ":";
    stream << std::setw(2) << std::setfill('0') << sec << ".";
    stream << std::setw(3) << std::setfill('0') << (stamp.toNSec()*(0.000001));
    return QString::fromStdString(stream.str());
}

void robotStatus::recievedMessage(const flor_ocs_msgs::OCSRobotStatus::ConstPtr& msg)
{

    //extract information from msg
    int type = msg->code >> 6;
    int msgNum = msg->code - type*64;
    std::cout << "Recieved message. type = " << type << "msgNum = " << msgNum << std::endl;
    QTableWidgetItem* text = new QTableWidgetItem();
    QTableWidgetItem* msgType = new QTableWidgetItem();
    QTableWidgetItem* time = new QTableWidgetItem();
    time->setText(timeFromMsg(msg->stamp));

    switch(type){
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

    if(msgNum >= errors.size() && errors.size() != 0)
    {
        QString tempMessage = "Recieved message number";
        tempMessage+=QString::number(msgNum);
        tempMessage += QString::fromStdString(" not in list");
        text->setText(tempMessage);
        text->setBackgroundColor(Qt::red);
        time->setBackgroundColor(Qt::red);
        msgType->setBackgroundColor(Qt::red);
        numError++;
    }
    else if(errors.size() > 0)
        text->setText(QString::fromStdString(errors[msgNum]));
    else
    {
        QString tempMessage = "Cannot find data file but recieved msg num ";
        tempMessage+= QString::number(msgNum);
        text->setText(tempMessage);
        text->setBackground(Qt::red);
        msgType->setBackgroundColor(Qt::red);
        time->setBackgroundColor(Qt::red);
        numError++;
    }

    msgType->setFont(bold);
    time->setFont(bold);
    text->setFont(bold);
    msgTable->insertRow(msgTable->rowCount());
    messages.push_back(new completeRow());
    messages[messages.size()-1]->time = time;
    messages[messages.size()-1]->priority = msgType;
    messages[messages.size()-1]->text = text;
    std::cout << "Adding item to table..." << std::endl;
    msgTable->setItem(msgTable->rowCount()-1,0,messages[messages.size()-1]->time);
    msgTable->setItem(msgTable->rowCount()-1,1,messages[messages.size()-1]->priority);
    msgTable->setItem(msgTable->rowCount()-1,2,messages[messages.size()-1]->text);
    std::cout << "Item added sucessfuly..." << std::endl;
    unreadMsgs++;
}

void robotStatus::loadFile()
{
    std::cout << "Reading in csv File for error list at " << messagesFile.fileName().toStdString() << std::endl;
    errors.resize(RobotStatusCodes::MAX_ERROR_MESSAGES,"Default Error Message");
    QStringList splitList;
    if(messagesFile.open(QIODevice::ReadOnly))
    {
        QString data;
        data = messagesFile.readAll();
        splitList = data.split(',');
        messagesFile.close();
        std::cout << "Done reading in file" << std::endl;
    }
    else
        std::cout << "Failed to read in file" << std::endl;
    for(int index=0;index<splitList.size(); index++)
    {

        //std::cout << "index " << index << "= " << splitList.at(index).toStdString() << " Size = " << splitList.at(index).length() << std::endl;
        QString token = splitList.at(index);
        if(token.size() == 1 || token.at(0) == '#');
        else if(token.toInt() <= errors.size() && token.toInt() >=0)
        {
            errors[token.toInt()] = splitList.at(index+1).toStdString();
            index++;
        }
    }
 }

void robotStatus::on_clearButton_clicked()
{
    std::cout << "Clear button pressed..." << std::endl;
   // msgTable->hideRow(0);
    //msgTable = new QTableWidget();
    //msgTable->setColumnCount(2);
    //msgTable->setColumnWidth(0,50);
    //msgTable->setHorizontalHeaderLabels(labels);
    msgTable->clearContents();
    while(msgTable->rowCount() > 0)
        msgTable->removeRow(0);
    messages.clear();
    unreadMsgs=0;
    numError = 0;
    numWarn = 0;
}

void robotStatus::on_msgTable_cellClicked(int row, int column)
{
    std::cout << "row " << row << " clicked to clear" <<std::endl;
    if((messages[row])->priority->text() == "Warn")
        numWarn--;
    if((messages[row])->priority->text() == "Error")
        numError--;
    unreadMsgs--;
    (messages[row])->time->setFont(normal);
    (messages[row])->priority->setFont(normal);
    (messages[row])->text->setFont(normal);
}

int robotStatus::getNumUnread()
{
    return unreadMsgs;
}
int robotStatus::getNumError()
{
    return numError;
}
int robotStatus::getNumWarn()
{
    return numWarn;
}

robotStatus::~robotStatus()
{
    //delete ui;
}

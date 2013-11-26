#include "robotStatus.h"
#include <QVBoxLayout>
#include <ros/ros.h>
#include <flor_ocs_msgs/RobotStatusCodes.h>

robotStatus::robotStatus(QWidget *parent) :
    QWidget(parent)
{
    maxRows = 100;

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
    //labels = new QStringList();opt/vigir/catkin_ws/src/flor_common/flor_ocs_msgs/include/flor_ocs_msgs/
    labels.push_back("Sim Time");
    labels.push_back("Type");
    labels.push_back("Message Contents");
    msgTable->setHorizontalHeaderLabels(labels);
    unreadMsgs=0;
    numError = 0;
    numWarn = 0;
    ros::NodeHandle nh("~");
    std::string fileName;
    if(nh.getParam("robotErrorFileLocation",fileName))
        messagesPath = fileName;
    else
        messagesPath = "/opt/vigir/catkin_ws/src/flor_common/flor_ocs_msgs/include/flor_ocs_msgs/messages.csv";

    std::cerr << "Reading messages from <" << messagesPath << ">" << std::endl;
    loadFile();

    rosSubscriber = nh_.subscribe<flor_ocs_msgs::OCSRobotStatus>( "/flor_robot_status", 100, &robotStatus::receivedMessage, this );
    clearCalled   = nh_.subscribe<std_msgs::Bool>("/flor_robot_status/clear", 1, &robotStatus::clearCalledMsg, this);
    callClear_pub = nh_.advertise<std_msgs::Bool>("/flor_robot_status/clear",1,false);
    key_event_sub_ = nh_.subscribe<flor_ocs_msgs::OCSKeyEvent>( "/flor/ocs/key_event", 5, &robotStatus::processNewKeyEvent, this );
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

robotStatus::~robotStatus()
{
    //delete ui;
}

void robotStatus::timerEvent(QTimerEvent *event)
{
    // check if ros is still running; if not, just kill the application
    if(!ros::ok())
        qApp->quit();
    
    //Spin at beginning of Qt timer callback, so current ROS time is retrieved
    ros::spinOnce();
}

void robotStatus::on_radioButtons_updated()
{
    std::cout <<"Radio buttons changed updating list..." <<std::endl;
    for(int i=0;i<messages.size();i++)
    {
        msgTable->hideRow(i);
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
    }
}

QString robotStatus::timeFromMsg(ros::Time stamp)
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

void robotStatus::receivedMessage(const flor_ocs_msgs::OCSRobotStatus::ConstPtr& msg)
{

    //extract information from msg
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
    msgTable->insertRow(0);
    //std::cout << "Adding item to table... " << messages.size() <<  " " << messages[0]->text << std::endl;
    msgTable->setItem(0,0,messages[0]->time);
    msgTable->setItem(0,1,messages[0]->priority);
    msgTable->setItem(0,2,messages[0]->text);

    if(messages[0]->priority->text() == "Ok" && showOk->isChecked())
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
    //std::cout << "Item added sucessfuly..." << std::endl;
    if(messages.size() > maxRows)
    {
        if(messages[messages.size()-1]->priority->text() == "Warn")
            numWarn--;
        else if(messages[messages.size()-1]->priority->text() == "Error")
            numError--;
        messages.pop_back();
        msgTable->removeRow(maxRows);
    }
    unreadMsgs++;
}

void robotStatus::loadFile()
{
    errors.resize(RobotStatusCodes::MAX_ERROR_MESSAGES,"Default Error Message");
    QFile file(QString::fromStdString(messagesPath));
    std::cout << "Trying to open file at " << messagesPath << std::endl;
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        std::cout << "File opened successfully... now parsing. Will print valid messages" << std::endl;
        QTextStream in(&file);
        while(!in.atEnd())
        {
            QString line = in.readLine();
            if(line[0] != '#')
            {
                QStringList strings;
                strings = line.split(',');
                if(strings.size() > 1)
                {
                    errors[strings[0].toInt()] = strings[1].toStdString();
                    std::cout << "Msg # " << strings[0].toStdString() << ":" << strings[1].toStdString() <<std::endl;
                }
            }
        }
    }
}
void robotStatus::clearCalledMsg(const std_msgs::Bool::ConstPtr& msg)
{
    std::cout << "Clear called by another status window. Now clearing my window." << std::endl;
    clearTable();
}

void robotStatus::on_clearButton_clicked()
{
    std::cout << "Clear button pressed. Clearing local window and informing other instances to clear as well." << std::endl;
    // msgTable->hideRow(0);
    //msgTable = new QTableWidget();
    //msgTable->setColumnCount(2);
    //msgTable->setColumnWidth(0,50);
    //msgTable->setHorizontalHeaderLabels(labels);
    clearTable();
    std_msgs::Bool foo;
    callClear_pub.publish(foo);
}

void robotStatus::clearTable()
{
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

void robotStatus::processNewKeyEvent(const flor_ocs_msgs::OCSKeyEvent::ConstPtr &key_event)
{
    // store key state
    if(key_event->state)
        keys_pressed_list_.push_back(key_event->key);
    else
        keys_pressed_list_.erase(std::remove(keys_pressed_list_.begin(), keys_pressed_list_.end(), key_event->key), keys_pressed_list_.end());

    // process hotkeys
    std::vector<int>::iterator key_is_pressed;

    key_is_pressed = std::find(keys_pressed_list_.begin(), keys_pressed_list_.end(), 37);
    if(key_event->key == 19 && key_event->state && key_is_pressed != keys_pressed_list_.end()) // ctrl+0
    {
        if(this->isVisible())
        {
            this->hide();
        }
        else
        {
            //this->move(QPoint(key_event->cursor_x+5, key_event->cursor_y+5));
            this->show();
        }
    }
}

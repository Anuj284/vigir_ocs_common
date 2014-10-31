#include "graspWidget.h"
#include "ui_graspWidget.h"
#include <ros/package.h>
#include <algorithm>
#include <QColor>
#include <QProgressBar>
#include <QSlider>

//grasp_testing grasp_testing_simple.launch

graspWidget::graspWidget(QWidget *parent, std::string hand, std::string hand_type)
    : QWidget(parent)
    , ui(new Ui::graspWidget)
    , selected_template_id_(-1)
    , selected_grasp_id_(-1)
    , show_grasp_(false)
    , stitch_template_(false)
    , hand_(hand)
    , hand_type_(hand_type)
{

    std::string ip = ros::package::getPath("vigir_ocs_grasp_widget")+"/icons/";
    icon_path_ = QString(ip.c_str());

    // setup UI
    ui->setupUi(this);
    ui->templateBox->setDisabled(true);
    ui->graspBox->setDisabled(true);
    ui->performButton->setDisabled(true);
    ui->stitch_template->setDisabled(true);
    setUpButtons();

    // these are not parameters anymore, but arguments in the constructor
    //ros::NodeHandle nhp("~");
    //nhp.param<std::string>("hand",hand_,"left"); // private parameter
    //nhp.param<std::string>("hand_type",hand_type_,"irobot"); // global parameter
    //ROS_ERROR("  Grasp widget using %s hand (%s)",hand_.c_str(), hand_type_.c_str());

    // initialize path variables for template/grasp databases
    std::string templatePath = (ros::package::getPath("vigir_template_library"))+"/";
    std::cout << "--------------<" << templatePath << ">\n" << std::endl;
    template_dir_path_ = QString(templatePath.c_str());
    template_id_db_path_ = template_dir_path_+QString("grasp_templates.txt");
    if(hand_type_ == "irobot"){
        grasp_db_path_ = template_dir_path_+QString("grasp_library_irobot.grasp");
    }
    else if(hand_type_ == "robotiq"){
        grasp_db_path_ = template_dir_path_+QString("grasp_library_robotiq.grasp");
    }
    else{
        grasp_db_path_ = template_dir_path_+QString("grasp_library.grasp");
        ui->label_9->setText("Index");
        ui->label_10->setText("Middle");
        ui->label_11->setText("Pinky");
    }

    this->stitch_template_pose_.setIdentity();

    this->hand_offset_pose_.setIdentity();

    // initialize variables
    currentGraspMode = 0;
    templateMatchDone = false;

    // read from databases
    initTemplateIdMap();
    initGraspDB();

    std::string grasp_control_prefix = (hand_ == "left") ? "/grasp_control/l_hand" : "/grasp_control/r_hand";
    // initialize template subscribers and publishers
    template_list_sub_           = nh_.subscribe<flor_ocs_msgs::OCSTemplateList>(    "/template/list",                    5, &graspWidget::processTemplateList, this );
    template_match_feedback_sub_ = nh_.subscribe<flor_grasp_msgs::TemplateSelection>("/grasp_control/template_selection", 1, &graspWidget::templateMatchFeedback, this );
    grasp_state_sub_             = nh_.subscribe<flor_grasp_msgs::GraspState>(       grasp_control_prefix+"/active_state",            1, &graspWidget::graspStateReceived,  this );

    grasp_selection_pub_        = nh_.advertise<flor_grasp_msgs::GraspSelection>(    grasp_control_prefix+"/grasp_selection",        1, false);
    grasp_release_pub_          = nh_.advertise<flor_grasp_msgs::GraspSelection>(    grasp_control_prefix+"/release_grasp" ,         1, false);
    grasp_mode_command_pub_     = nh_.advertise<flor_grasp_msgs::GraspState>(        grasp_control_prefix+"/mode_command",     1, false);

    // create subscribers for grasp status
    std::stringstream finger_joint_name;
    XmlRpc::XmlRpcValue   hand_T_palm;

    float color_r, color_g, color_b;
    if(hand_ == "left")
    {
        this->setWindowTitle(QString::fromStdString("Left Hand Grasp Widget"));

        //Publisher for template match rewuest for LEFT
        template_match_request_pub_ = nh_.advertise<flor_grasp_msgs::TemplateSelection>( "/template/l_hand_template_match_request", 1, false );

        robot_status_sub_           = nh_.subscribe<flor_ocs_msgs::OCSRobotStatus>( "/grasp_control/l_hand/grasp_status",1, &graspWidget::robotStatusCB,  this );
        ghost_hand_pub_             = nh_.advertise<geometry_msgs::PoseStamped>(     "/ghost_left_hand_pose",             1, false);
        //ghost_hand_joint_state_pub_ = nh_.advertise<sensor_msgs::JointState>(        "/ghost_left_hand/joint_states",     1, false); // /ghost_left_hand/joint_states

        hand_model_loader_.reset(new robot_model_loader::RobotModelLoader("left_hand_robot_description"));
        hand_robot_model_ = hand_model_loader_->getModel();
        hand_robot_state_.reset(new robot_state::RobotState(hand_robot_model_));
        // Publisher for hand position/state
        robot_state_vis_pub_ = nh_.advertise<moveit_msgs::DisplayRobotState>("/flor/ghost/template_left_hand",1, true);

        // We first subscribe to the JointState messages
        link_states_sub_ = nh_.subscribe<flor_grasp_msgs::LinkState>( "/grasp_control/l_hand/tactile_feedback", 2, &graspWidget::linkStatesCB, this );

        template_stitch_pose_sub_    = nh_.subscribe<geometry_msgs::PoseStamped>( "/grasp_control/l_hand/template_stitch_pose",1, &graspWidget::templateStitchPoseCallback,  this );
        template_stitch_request_pub_ = nh_.advertise<flor_grasp_msgs::TemplateSelection>( "/grasp_control/l_hand/template_stitch_request", 1, false );

        if(nh_.getParam("/l_hand_tf/hand_T_palm", hand_T_palm))
        {
            hand_T_palm_.setOrigin(tf::Vector3(static_cast<double>(hand_T_palm[0]),static_cast<double>(hand_T_palm[1]),static_cast<double>(hand_T_palm[2])));
            hand_T_palm_.setRotation(tf::Quaternion(static_cast<double>(hand_T_palm[3]),static_cast<double>(hand_T_palm[4]),static_cast<double>(hand_T_palm[5]),static_cast<double>(hand_T_palm[6])));
        }
        else
        {
            hand_T_palm_.setOrigin(tf::Vector3(0,0,0));
            hand_T_palm_.setRotation(tf::Quaternion(0,0,0,1));
        }

        if(nh_.getParam("/l_hand_tf/gp_T_palm", hand_T_palm))
        {
            gp_T_palm_.setOrigin(tf::Vector3(static_cast<double>(hand_T_palm[0]),static_cast<double>(hand_T_palm[1]),static_cast<double>(hand_T_palm[2])));
            gp_T_palm_.setRotation(tf::Quaternion(static_cast<double>(hand_T_palm[3]),static_cast<double>(hand_T_palm[4]),static_cast<double>(hand_T_palm[5]),static_cast<double>(hand_T_palm[6])));
        }
        else
        {
            gp_T_palm_.setOrigin(tf::Vector3(0,0,0));
            gp_T_palm_.setRotation(tf::Quaternion(0,0,0,1));
        }

        color_r = 1.0f;
        color_g = 1.0f;
        color_b = 0.0f;

        planning_hand_target_pub_   = nh_.advertise<geometry_msgs::PoseStamped>( "/grasp_control/l_hand/planning_target_pose", 1, false );

        hand_offset_sub_    = nh_.subscribe<geometry_msgs::PoseStamped>( "/template/l_hand_template_offset",1, &graspWidget::handOffsetCallback,  this );
    }
    else
    {
        this->setWindowTitle(QString::fromStdString("Right Hand Grasp Widget"));

        //Publisher for template match rewuest for RIGHT
        template_match_request_pub_ = nh_.advertise<flor_grasp_msgs::TemplateSelection>( "/template/r_hand_template_match_request", 1, false );

        robot_status_sub_           = nh_.subscribe<flor_ocs_msgs::OCSRobotStatus>( "/grasp_control/r_hand/grasp_status",1, &graspWidget::robotStatusCB,  this );
        ghost_hand_pub_             = nh_.advertise<geometry_msgs::PoseStamped>(     "/ghost_right_hand_pose",            1, false);
        //ghost_hand_joint_state_pub_ = nh_.advertise<sensor_msgs::JointState>(        "/ghost_right_hand/joint_states",    1, false); // /ghost_right_hand/joint_states

        hand_model_loader_.reset(new robot_model_loader::RobotModelLoader("right_hand_robot_description"));
        hand_robot_model_ = hand_model_loader_->getModel();
        hand_robot_state_.reset(new robot_state::RobotState(hand_robot_model_));
        // Publisher for hand position/state
        robot_state_vis_pub_ = nh_.advertise<moveit_msgs::DisplayRobotState>("/flor/ghost/template_right_hand",1, true);

        // We first subscribe to the JointState messages
        link_states_sub_ = nh_.subscribe<flor_grasp_msgs::LinkState>( "/grasp_control/r_hand/tactile_feedback", 2, &graspWidget::linkStatesCB, this );

        template_stitch_pose_sub_    = nh_.subscribe<geometry_msgs::PoseStamped>( "/grasp_control/r_hand/template_stitch_pose",1, &graspWidget::templateStitchPoseCallback,  this );
        template_stitch_request_pub_ = nh_.advertise<flor_grasp_msgs::TemplateSelection>( "/grasp_control/r_hand/template_stitch_request", 1, false );

        if(nh_.getParam("/r_hand_tf/hand_T_palm", hand_T_palm))
        {
            hand_T_palm_.setOrigin(tf::Vector3(static_cast<double>(hand_T_palm[0]),static_cast<double>(hand_T_palm[1]),static_cast<double>(hand_T_palm[2])));
            hand_T_palm_.setRotation(tf::Quaternion(static_cast<double>(hand_T_palm[3]),static_cast<double>(hand_T_palm[4]),static_cast<double>(hand_T_palm[5]),static_cast<double>(hand_T_palm[6])));
        }
        else
        {
            hand_T_palm_.setOrigin(tf::Vector3(0,0,0));
            hand_T_palm_.setRotation(tf::Quaternion(0,0,0,1));
        }

        if(nh_.getParam("/r_hand_tf/gp_T_palm", hand_T_palm))
        {
            gp_T_palm_.setOrigin(tf::Vector3(static_cast<double>(hand_T_palm[0]),static_cast<double>(hand_T_palm[1]),static_cast<double>(hand_T_palm[2])));
            gp_T_palm_.setRotation(tf::Quaternion(static_cast<double>(hand_T_palm[3]),static_cast<double>(hand_T_palm[4]),static_cast<double>(hand_T_palm[5]),static_cast<double>(hand_T_palm[6])));
        }
        else
        {
            gp_T_palm_.setOrigin(tf::Vector3(0,0,0));
            gp_T_palm_.setRotation(tf::Quaternion(0,0,0,1));
        }

        color_r = 0.0f;
        color_g = 1.0f;
        color_b = 1.0f;

        planning_hand_target_pub_   = nh_.advertise<geometry_msgs::PoseStamped>( "/grasp_control/r_hand/planning_target_pose", 1, false );

        hand_offset_sub_    = nh_.subscribe<geometry_msgs::PoseStamped>( "/template/r_hand_template_offset",1, &graspWidget::handOffsetCallback,  this );
    }
    // this is for publishing the hand position in world coordinates for moveit
    virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_x");
    virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_y");
    virtual_link_joint_states_.name.push_back("world_virtual_joint/trans_z");
    virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_x");
    virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_y");
    virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_z");
    virtual_link_joint_states_.name.push_back("world_virtual_joint/rot_w");
    virtual_link_joint_states_.position.resize(7);

    // publisher to color the hand links
    hand_link_color_pub_        = nh_.advertise<flor_ocs_msgs::OCSLinkColor>("/link_color", 1, false);

    // find robot status message code csv file
    std::string code_path_ = (ros::package::getPath("flor_ocs_msgs"))+"/include/flor_ocs_msgs/messages.csv";
    std::cout << code_path_ << std::endl;
    robot_status_codes_.loadErrorMessages(code_path_);

    // change color of the ghost template hands
    const std::vector<std::string>& link_names = hand_robot_model_->getLinkModelNames();

    for (size_t i = 0; i < link_names.size(); ++i)
    {
        moveit_msgs::ObjectColor tmp;
        tmp.id = link_names[i];
        tmp.color.a = 0.5f;
        tmp.color.r = color_r;
        tmp.color.g = color_g;
        tmp.color.b = color_b;
        display_state_msg_.highlight_links.push_back(tmp);
    }

    // create publisher and subscriber for object selection
    // PUBLISHER WILL BE USED BY THE RIGHT/DOUBLE CLICK TO INFORM WHICH TEMPLATE/HAND/OBJECT HAS BEEN selected
    // SUBSCRIBER WILL BE USED TO CHANGE VISIBILITY OF THE OBJECT THAT IS BEING USED (E.G., TALK TO TEMPLATE DISPLAY AND SET VISIBILITY OF MARKERS)
    select_object_pub_ = nh_.advertise<flor_ocs_msgs::OCSObjectSelection>( "/flor/ocs/object_selection", 1, false );
    select_object_sub_ = nh_.subscribe<flor_ocs_msgs::OCSObjectSelection>( "/flor/ocs/object_selection", 5, &graspWidget::processObjectSelection, this );

    key_event_sub_ = nh_.subscribe<flor_ocs_msgs::OCSKeyEvent>( "/flor/ocs/key_event", 5, &graspWidget::processNewKeyEvent, this );
    timer.start(33, this);
}
//SetStylesheet to change on the fly


graspWidget::~graspWidget()
{
    ui2->close();
    delete ui;
}

void graspWidget::timerEvent(QTimerEvent *event)
{
	// check if ros is still running; if not, just kill the application
    if(!ros::ok())
        qApp->quit();
    
    // make sure that we don't show the grasp hands if the user doesn't want to see them
    if(!ui->graspBox->isEnabled() || !show_grasp_)
        hideHand();

    //Spin at beginning of Qt timer callback, so current ROS time is retrieved
    ros::spinOnce();
}

void graspWidget::setUpButtons()
{
    //set button style
    QString btnStyle = QString("QPushButton  { ") +
                               " background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(240, 240, 240, 255), stop:1 rgba(222, 222, 222, 255));" +
                               " border-style: solid;" +
                               " border-width: 1px;" +
                               " border-radius: 1px;" +
                               " border-color: gray;" +
                               " padding: 2px;" +
                               " image-position: top left"
                               "}" +

                               "QPushButton:checked  {" +
                               " padding-top:1px; padding-left:1px;" +
                               " background-color: rgb(180,180,180);" +
                               " border-style: inset;" +
                               "}";
    ui->releaseButton->setStyleSheet(btnStyle);
    ui->releaseButton->setFont(QFont ("Ubuntu", 10));
    ui->performButton->setStyleSheet(btnStyle);
    ui->performButton->setFont(QFont ("Ubuntu", 10));
    ui->templateButton->setStyleSheet(btnStyle);
    ui->templateButton->setFont(QFont ("Ubuntu", 10));
    ui->graspOffsetButton->setStyleSheet(btnStyle);
    ui->graspOffsetButton->setFont(QFont ("Ubuntu", 10));
    //put arrows on comboboxes
    QString styleSheet = ui->templateBox->styleSheet() + "\n" +
            "QComboBox::down-arrow {\n" +
            " image: url(" + icon_path_ + "down_arrow.png" + ");\n" +
            "}";
    ui->templateBox->setStyleSheet(styleSheet);
    ui->graspBox->setStyleSheet(styleSheet);
}

void graspWidget::templateMatchFeedback (const flor_grasp_msgs::TemplateSelection::ConstPtr& feedback)
{
    // provide feedback about template grasp confidence by changing the color of the move to template button
    QPalette pal = ui->templateButton->palette();
    if(feedback->confidence.data >= 85)
        pal.setColor(QPalette::Button,Qt::green);
    else if(feedback->confidence.data >=60)
        pal.setColor(QPalette::Button,Qt::yellow);
    else
        pal.setColor(QPalette::Button,Qt::red);
    ui->templateButton->setPalette(pal);
    ui->templateButton->setAutoFillBackground(true);
    std::cout << "Template confidence received and found to be " << (int)feedback->confidence.data << std::endl;
    templateMatchDone = true; // is this still being used?
}

void graspWidget::graspStateReceived (const flor_grasp_msgs::GraspState::ConstPtr& graspState)
{
    //std::cout << "Grasp State message received" << graspState << std::endl;
    uint8_t mode  = (graspState->grasp_state.data&0xF0) >> 4;
    uint8_t state = graspState->grasp_state.data&0x0F;
    setProgressLevel(graspState->grip.data);

    ui->userSlider->setValue(graspState->grip.data);
    ui->verticalSlider->setValue(graspState->finger_effort[0].data);
    ui->verticalSlider_2->setValue(graspState->finger_effort[1].data);
    ui->verticalSlider_3->setValue(graspState->finger_effort[2].data);
    ui->verticalSlider_4->setValue(graspState->finger_effort[3].data);
    //std::cout << "     mode=" << uint32_t(mode) << "   state="<< uint32_t(state) << std::endl;
    switch(mode)
    {
    case flor_grasp_msgs::GraspState::GRASP_MODE_NONE:
        ui->currentStateLabel->setText("idle");
        currentGraspMode = 0;
        break;
    case flor_grasp_msgs::GraspState::TEMPLATE_GRASP_MODE:
        if(currentGraspMode != 1)
        {
            ui->manualRadio->setChecked(false);
            ui->templateRadio->setChecked(true);
            initTemplateMode();
            currentGraspMode = 1;
        }
        switch(state)
        {
        case flor_grasp_msgs::GraspState::GRASP_STATE_NONE:
            ui->currentStateLabel->setText("State Unknown");
            break;
        case flor_grasp_msgs::GraspState::GRASP_INIT:
            ui->currentStateLabel->setText("init");
            break;
        case flor_grasp_msgs::GraspState::APPROACHING:
            ui->currentStateLabel->setText("approaching");
            break;
        case flor_grasp_msgs::GraspState::SURROUNDING:
            ui->currentStateLabel->setText("surrounding");
            break;
        case flor_grasp_msgs::GraspState::GRASPING:
            ui->currentStateLabel->setText("grasping");
            break;
        case flor_grasp_msgs::GraspState::MONITORING:
            ui->currentStateLabel->setText("monitoring");
            break;
        case flor_grasp_msgs::GraspState::OPENING:
            ui->currentStateLabel->setText("opening");
            break;
        case flor_grasp_msgs::GraspState::GRASP_ERROR:
        default:
            ui->currentStateLabel->setText("template error");
            break;
        }
        break;
    case flor_grasp_msgs::GraspState::MANUAL_GRASP_MODE:
        if(currentGraspMode != 2)
        {
            ui->manualRadio->setChecked(true);
            ui->templateRadio->setChecked(false);
            currentGraspMode = 2;
        }
        ui->currentStateLabel->setText("manual");
        break;
    default:
        ui->currentStateLabel->setText("MODE ERROR");
        break;
    }
}


void graspWidget::processTemplateList( const flor_ocs_msgs::OCSTemplateList::ConstPtr& list)
{
    //std::cout << "Template list received containing " << list->template_id_list.size() << " elements" << std::cout;
    // save last template list
    last_template_list_ = *list;

    // enable boxes and buttons
    if(list->template_list.size() > 0 && selected_template_id_ != -1)
    {
        //ui->templateBox->setDisabled(false);
        ui->graspBox->setDisabled(false);
        ui->performButton->setDisabled(false);
    }

    bool was_empty = ui->templateBox->count() == 0 ? true : false;

    QString currentItem = ui->templateBox->currentText();
    //ui->templateBox->clear();

    // populate template combobox
    for(int i = 0; i < list->template_list.size(); i++)
    {
        // remove the .mesh string from the template name
        std::string templateName = list->template_list[i];
        if(templateName.size() > 5 && templateName.substr(templateName.size()-5,5) == ".mesh")
            templateName = templateName.substr(0,templateName.size()-5);
        // add the template
        templateName = boost::to_string((int)list->template_id_list[i])+std::string(": ")+templateName;

        //std::cout << "template item " << (int)list->template_id_list[i] << " has name " << templateName << std::endl;

        // add the template to the box if it doesn't exist
        if(ui->templateBox->count() < i+1)
        {
            ui->templateBox->addItem(QString::fromStdString(templateName));
        } // update existing templates
        else if( ui->templateBox->itemText(i).toStdString() != templateName)
        {
            ui->templateBox->setItemText(i,QString::fromStdString(templateName));
        }
    }

    for(int i = list->template_list.size(); i < ui->templateBox->count(); i++)
        ui->templateBox->removeItem(i);

    if(selected_template_id_ != -1 && ui->templateBox->findText(currentItem) == -1)
    {
        hideHand();
        ui->graspBox->clear();
        selected_template_id_ = -1;
        selected_grasp_id_ = -1;
        ui->graspBox->setEnabled(false);
    }
    else
    {
        if(was_empty && ui->templateBox->count() > 0)
        {
            //ROS_ERROR("Seleting template 0");
            ui->templateBox->setCurrentIndex(0);
            on_templateBox_activated(ui->templateBox->itemText(0));
            on_templateRadio_clicked();
            selected_template_id_ = 0;
        }

        if(selected_grasp_id_ != -1 && show_grasp_)
            publishHandPose(selected_grasp_id_);
    }
}

void graspWidget::initTemplateMode()
{
    if(last_template_list_.template_id_list.size() > 0)
    {
        //ui->templateBox->setDisabled(false);
        ui->graspBox->setDisabled(false);
    }
}

//currentStateLabel
void graspWidget::setProgressLevel(uint8_t level)
{
    //std::cout << "setting fill level to be " << (int)level << std::endl;
    if(level >=100)
    {
        ui->closedGraph->setValue(100);
        ui->forceGraph->setValue((int)(level-100));
    }
    else
    {
        ui->closedGraph->setValue(level);
        ui->forceGraph->setValue(0);
    }
}

void graspWidget::initTemplateIdMap()
{
    std::vector< std::vector<QString> > db = readTextDBFile(template_id_db_path_);

    for(int i = 0; i < db.size(); i++)
    {
        TemplateDBItem template_item;
        bool ok;
        unsigned char id = db[i][0].toUInt(&ok, 10) & 0x000000ff;
        std::string templatePath(db[i][1].toUtf8().constData());
        std::cout << "-> Adding template (" << templatePath << ") to id (" << (unsigned int)id << ") map" << std::endl;
        template_id_map_.insert(std::pair<unsigned char,std::string>(id,templatePath));
        geometry_msgs::Point com ;
        com.x = db[i][8].toFloat(&ok);
        com.y = db[i][9].toFloat(&ok);
        com.z = db[i][10].toFloat(&ok);
        double mass = db[i][11].toFloat(&ok);
        template_item.com  = com;
        template_item.mass = mass;
        template_item.template_type = id;
        template_db_.push_back(template_item);
    }
}

// will return a vector with rows, each row containing a QStringList with all columns
std::vector< std::vector<QString> > graspWidget::readTextDBFile(QString path)
{
    std::vector< std::vector<QString> > ret;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            if(line[0] != '#')
            {
                std::vector<QString> row;
                QStringList strings;
                strings = line.split(",");
                // remove whitespaces
                for(int i = 0; i < strings.size(); i++)
                {
                    QString str = strings.at(i);
                    str.replace(" ","");
                    row.push_back(str);
                }
                ret.push_back(row);
            }
        }
    }
    return ret;
}

void graspWidget::initGraspDB()
{
    if(hand_type_ == "irobot")
    {
        std::vector< std::vector<QString> > db = readTextDBFile(grasp_db_path_);
        for(int i = 0; i < db.size(); i++)
        {
            bool ok;
            // [0] grasp id, [1] template type, [2] hand, [3] initial grasp type, [4] DISCARD, [5-9] finger joints (5), [10] DISCARD, [11-17] final grasp pose relative to template (x,y,z,qx,qy,qz,qw), [18] DISCARD, [19-25] pre-grasp pose relative to template (x,y,z,qx,qy,qz,qw)
            GraspDBItem grasp;
            //std::cout << "-> Adding grasp to grasp DB" << std::endl;
            grasp.grasp_id = db[i][0].toUInt(&ok, 10) & 0x0000ffff;
            std::cout << "id: " << (unsigned int)grasp.grasp_id << std::endl;

            grasp.template_type = db[i][1].toUInt(&ok, 10) & 0x000000ff;
            std::cout << "template type: " << (unsigned int)grasp.template_type << std::endl;

            grasp.template_name = template_id_map_.find(grasp.template_type)->second;
            std::cout << "template name: " << grasp.template_name << std::endl;

            grasp.hand = db[i][2].toUtf8().constData();
            std::cout << "hand: " << grasp.hand << std::endl;

            grasp.initial_grasp_type = db[i][3].toUtf8().constData();
            std::cout << "initial grasp type: " << grasp.initial_grasp_type << std::endl;

            //std::cout << "finger joints: ";
            for(int j = 0; j < 5; j++)
            {
                // need to ignore fourth joint in the grasp file [3], and send the same value as [4] in its place
                //                joint_states.name.push_back(hand+"_finger[0]_proximal");
                //                joint_states.name.push_back(hand+"_finger[1]_proximal");
                //                joint_states.name.push_back(hand+"_finger[2]_proximal");
                //                joint_states.name.push_back(hand+"_finger[0]_base_rotation"); // .grasp finger position [4] -> IGNORE [3], use [4] for both
                //                joint_states.name.push_back(hand+"_finger[1]_base_rotation");// .grasp finger position [4]
                if(j == 3)
                    grasp.finger_joints[j] = db[i][j+5+1].toFloat(&ok);
                else
                    grasp.finger_joints[j] = db[i][j+5].toFloat(&ok);
            }
            // need to set distal joints as 0
            //                joint_states.name.push_back(hand+"_finger[0]_distal"); // 0 for now
            //                joint_states.name.push_back(hand+"_finger[1]_distal");// 0 for now
            //                joint_states.name.push_back(hand+"_finger[2]_distal");// 0 for now
            grasp.finger_joints[5] = 0;
            grasp.finger_joints[6] = 0;
            grasp.finger_joints[7] = 0;
            //std::cout << std::endl;

            grasp.final_pose.position.x = db[i][11].toFloat(&ok);
            grasp.final_pose.position.y = db[i][12].toFloat(&ok);
            grasp.final_pose.position.z = db[i][13].toFloat(&ok);
            grasp.final_pose.orientation.w = db[i][14].toFloat(&ok);
            grasp.final_pose.orientation.x = db[i][15].toFloat(&ok);
            grasp.final_pose.orientation.y = db[i][16].toFloat(&ok);
            grasp.final_pose.orientation.z = db[i][17].toFloat(&ok);
            //std::cout << "final pose: " << grasp.final_pose.position.x << ", " << grasp.final_pose.position.y << ", " << grasp.final_pose.position.z << ", " <<
            //             grasp.final_pose.orientation.x << ", " << grasp.final_pose.orientation.y << ", " << grasp.final_pose.orientation.y << ", " << grasp.final_pose.orientation.w << std::endl;

            grasp.pre_grasp_pose.position.x = db[i][19].toFloat(&ok);
            grasp.pre_grasp_pose.position.y = db[i][20].toFloat(&ok);
            grasp.pre_grasp_pose.position.z = db[i][21].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.w = db[i][22].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.x = db[i][23].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.y = db[i][24].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.z = db[i][25].toFloat(&ok);
            //std::cout << "final pose: " << grasp.pre_grasp_pose.position.x << ", " << grasp.pre_grasp_pose.position.y << ", " << grasp.pre_grasp_pose.position.z << ", " <<
            //             grasp.pre_grasp_pose.orientation.x << ", " << grasp.pre_grasp_pose.orientation.y << ", " << grasp.pre_grasp_pose.orientation.y << ", " << grasp.pre_grasp_pose.orientation.w << std::endl;

            grasp_db_.push_back(grasp);
        }
    }
    else if(hand_type_ == "robotiq")
    {
        std::vector< std::vector<QString> > db = readTextDBFile(grasp_db_path_);
        for(int i = 0; i < db.size(); i++)
        {
            bool ok;
            // [0] grasp id, [1] template type, [2] hand, [3] initial grasp type, [4] DISCARD, [5-9] finger joints (5), [10] DISCARD, [11-17] final grasp pose relative to template (x,y,z,qx,qy,qz,qw), [18] DISCARD, [19-25] pre-grasp pose relative to template (x,y,z,qx,qy,qz,qw)
            GraspDBItem grasp;
            //std::cout << "-> Adding grasp to grasp DB" << std::endl;
            grasp.grasp_id = db[i][0].toUInt(&ok, 10) & 0x0000ffff;
            std::cout << "id: " << (unsigned int)grasp.grasp_id << std::endl;

            grasp.template_type = db[i][1].toUInt(&ok, 10) & 0x000000ff;
            std::cout << "template type: " << (unsigned int)grasp.template_type << std::endl;

            grasp.template_name = template_id_map_.find(grasp.template_type)->second;
            std::cout << "template name: " << grasp.template_name << std::endl;

            grasp.hand = db[i][2].toUtf8().constData();
            std::cout << "hand: " << grasp.hand << std::endl;

            grasp.initial_grasp_type = db[i][3].toUtf8().constData();
            std::cout << "initial grasp type: " << grasp.initial_grasp_type << std::endl;

            //std::cout << "finger joints: ";
            for(int j = 0; j < 4; j++)
            {
                grasp.finger_joints[j] = db[i][j+5].toFloat(&ok);
            }
            // need to set distal joints as 0
            grasp.finger_joints[4]  = -grasp.finger_joints[3];
            grasp.finger_joints[5]  = 0;
            grasp.finger_joints[6]  = 0;
            grasp.finger_joints[7]  = 0;
            grasp.finger_joints[8]  = 0;
            grasp.finger_joints[9]  = 0;
            grasp.finger_joints[10] = 0;
            //std::cout << std::endl;

            grasp.final_pose.position.x = db[i][10].toFloat(&ok);
            grasp.final_pose.position.y = db[i][11].toFloat(&ok);
            grasp.final_pose.position.z = db[i][12].toFloat(&ok);
            grasp.final_pose.orientation.w = db[i][13].toFloat(&ok);
            grasp.final_pose.orientation.x = db[i][14].toFloat(&ok);
            grasp.final_pose.orientation.y = db[i][15].toFloat(&ok);
            grasp.final_pose.orientation.z = db[i][16].toFloat(&ok);
            //std::cout << "final pose: " << grasp.final_pose.position.x << ", " << grasp.final_pose.position.y << ", " << grasp.final_pose.position.z << ", " <<
            //             grasp.final_pose.orientation.x << ", " << grasp.final_pose.orientation.y << ", " << grasp.final_pose.orientation.y << ", " << grasp.final_pose.orientation.w << std::endl;

            grasp.pre_grasp_pose.position.x = db[i][18].toFloat(&ok);
            grasp.pre_grasp_pose.position.y = db[i][19].toFloat(&ok);
            grasp.pre_grasp_pose.position.z = db[i][20].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.w = db[i][21].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.x = db[i][22].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.y = db[i][23].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.z = db[i][24].toFloat(&ok);
            //std::cout << "final pose: " << grasp.pre_grasp_pose.position.x << ", " << grasp.pre_grasp_pose.position.y << ", " << grasp.pre_grasp_pose.position.z << ", " <<
            //             grasp.pre_grasp_pose.orientation.x << ", " << grasp.pre_grasp_pose.orientation.y << ", " << grasp.pre_grasp_pose.orientation.y << ", " << grasp.pre_grasp_pose.orientation.w << std::endl;

            grasp_db_.push_back(grasp);
        }
    }
    else // sandia
    {
        std::vector< std::vector<QString> > db = readTextDBFile(grasp_db_path_);
        for(int i = 0; i < db.size(); i++)
        {
            bool ok;
            // [0] grasp id, [1] template type, [2] hand, [3] initial grasp type, [4] DISCARD, [5-16] finger joints (12), [17] DISCARD, [18-24] final grasp pose relative to template (x,y,z,qx,qy,qz,qw), [25] DISCARD, [26-32] pre-grasp pose relative to template (x,y,z,qx,qy,qz,qw)
            GraspDBItem grasp;
            //std::cout << "-> Adding grasp to grasp DB" << std::endl;
            grasp.grasp_id = db[i][0].toUInt(&ok, 10) & 0x0000ffff;
            std::cout << "id: " << (unsigned int)grasp.grasp_id << std::endl;

            grasp.template_type = db[i][1].toUInt(&ok, 10) & 0x000000ff;
            std::cout << "template type: " << (unsigned int)grasp.template_type << std::endl;

            grasp.template_name = template_id_map_.find(grasp.template_type)->second;
            std::cout << "template name: " << grasp.template_name << std::endl;

            grasp.hand = db[i][2].toUtf8().constData();
            std::cout << "hand: " << grasp.hand << std::endl;

            grasp.initial_grasp_type = db[i][3].toUtf8().constData();
            std::cout << "initial grasp type: " << grasp.initial_grasp_type << std::endl;

            //std::cout << "finger joints: ";
            for(int j = 0; j < 12; j++)
            {

                // Graspit outputs the fingers in different order, and these were copied into .grasp library
                // We need to swap f0 and f2, which this code does
                if(j < 3)
                    grasp.finger_joints[j+6] = db[i][j+5].toFloat(&ok);//grasp_spec.finger_poses.f2[i]= db[i][j].toFloat(&ok); //joints from the pinky
                else if (j > 5 && j < 9)
                    grasp.finger_joints[j-6] = db[i][j+5].toFloat(&ok);//grasp_spec.finger_poses.f0[i-6]= db[i][j].toFloat(&ok); //joints from the index
                else// if ((j-5) > 2 && (j-5) < 6)
                    grasp.finger_joints[j] = db[i][j+5].toFloat(&ok);//grasp_spec.finger_poses.f1[i-3]= db[i][j].toFloat(&ok); //joints from middle and thumb
                //else //9,10,11
                //    grasp.finger_joints[(j-5)] = db[i][j].toFloat(&ok);//grasp_spec.finger_poses.f3[i-9]= db[i][j].toFloat(&ok); //joints from thumb
                //grasp.finger_joints[j-5] = db[i][j].toFloat(&ok); // old code, using Graspit order
                //std::cout << grasp.finger_joints[j-5] << ",";
            }
            //std::cout << std::endl;

            grasp.final_pose.position.x = db[i][18].toFloat(&ok);
            grasp.final_pose.position.y = db[i][19].toFloat(&ok);
            grasp.final_pose.position.z = db[i][20].toFloat(&ok);
            grasp.final_pose.orientation.w = db[i][21].toFloat(&ok);
            grasp.final_pose.orientation.x = db[i][22].toFloat(&ok);
            grasp.final_pose.orientation.y = db[i][23].toFloat(&ok);
            grasp.final_pose.orientation.z = db[i][24].toFloat(&ok);
            //std::cout << "final pose: " << grasp.final_pose.position.x << ", " << grasp.final_pose.position.y << ", " << grasp.final_pose.position.z << ", " <<
            //             grasp.final_pose.orientation.x << ", " << grasp.final_pose.orientation.y << ", " << grasp.final_pose.orientation.y << ", " << grasp.final_pose.orientation.w << std::endl;

            grasp.pre_grasp_pose.position.x = db[i][26].toFloat(&ok);
            grasp.pre_grasp_pose.position.y = db[i][27].toFloat(&ok);
            grasp.pre_grasp_pose.position.z = db[i][28].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.w = db[i][29].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.x = db[i][30].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.y = db[i][31].toFloat(&ok);
            grasp.pre_grasp_pose.orientation.z = db[i][32].toFloat(&ok);
            //std::cout << "final pose: " << grasp.pre_grasp_pose.position.x << ", " << grasp.pre_grasp_pose.position.y << ", " << grasp.pre_grasp_pose.position.z << ", " <<
            //             grasp.pre_grasp_pose.orientation.x << ", " << grasp.pre_grasp_pose.orientation.y << ", " << grasp.pre_grasp_pose.orientation.y << ", " << grasp.pre_grasp_pose.orientation.w << std::endl;

            grasp_db_.push_back(grasp);
        }
    }
}


void graspWidget::sendManualMsg(uint8_t level, int8_t thumb,int8_t left,int8_t right ,int8_t spread)
{
    flor_grasp_msgs::GraspState cmd;
    cmd.grip.data         = level;
    cmd.finger_effort.resize(FINGER_EFFORTS);
    cmd.finger_effort[0].data = thumb;
    cmd.finger_effort[1].data = left;   //index for sandia
    cmd.finger_effort[2].data = right;  //middle for sandia
    cmd.finger_effort[3].data = spread; //Spread iRobot, Pinky for sandia
    cmd.grasp_state.data = 4; // leave as current command
    if (ui->graspBox->currentText() == QString("CYLINDRICAL"))  cmd.grasp_state.data = 0;
    if (ui->graspBox->currentText() == QString("PRISMATIC"))    cmd.grasp_state.data = 1;
    if (ui->graspBox->currentText() == QString("SPHERICAL"))    cmd.grasp_state.data = 2;
    cmd.grasp_state.data += (flor_grasp_msgs::GraspState::MANUAL_GRASP_MODE)<<4;
    grasp_mode_command_pub_.publish(cmd);
    std::cout << "Sent Manual mode message ("<< uint32_t(cmd.grasp_state.data) << ") with " <<  uint32_t(cmd.grip.data) << " manual grip level and " <<
                 int(cmd.finger_effort[0].data) << "," <<
                 int(cmd.finger_effort[1].data) << "," <<
                 int(cmd.finger_effort[2].data) << "," <<
                 int(cmd.finger_effort[3].data) <<
                 " finger efforts to " << hand_ << " hand" << std::endl;
}

void graspWidget::on_userSlider_sliderReleased()
{
    if(ui->manualRadio->isChecked())
    {
        setProgressLevel(ui->userSlider->value());
        sendManualMsg(ui->userSlider->value(), ui->verticalSlider->value(), ui->verticalSlider_2->value(), ui->verticalSlider_3->value(), ui->verticalSlider_4->value());
    }
    else if (ui->templateRadio->isChecked())
    {
        flor_grasp_msgs::GraspState msg;
        msg.finger_effort.resize(FINGER_EFFORTS);
        msg.finger_effort[0].data= ui->verticalSlider->value();
        msg.finger_effort[1].data= ui->verticalSlider_2->value();
        msg.finger_effort[2].data= ui->verticalSlider_3->value();
        msg.finger_effort[3].data= ui->verticalSlider_4->value();
        msg.grasp_state.data = ((flor_grasp_msgs::GraspState::TEMPLATE_GRASP_MODE)<<4) + 4;
        if(ui->userSlider->value() > 100)
        {
            msg.grip.data        = ui->userSlider->value();
            std::cout << "Adjust feedforward to " << int32_t(ui->userSlider->value()) << " with state=" << uint32_t(msg.grasp_state.data) << std::endl;
        }
        else
        {
            std::cout << "Only relevant in template mode if the feedforward is set!  New position is " << ui->userSlider->value() << std::endl;
            msg.grip.data        = 100; // can't undo grasp closure in template mode, but need to send message to clear feedforward
        }
        grasp_mode_command_pub_.publish(msg);
    }
    else
    {
        std::cout << "slider changed while not in any control mode. New position is " << ui->userSlider->value() << std::endl;
    }
}

void graspWidget::on_releaseButton_clicked()
{
    std::cout << "Release the grasp requested" << std::endl;
    ui->userSlider->setValue(0);
    ui->verticalSlider->setValue(0);
    ui->verticalSlider_2->setValue(0);
    ui->verticalSlider_3->setValue(0);
    ui->verticalSlider_4->setValue(0);
    //flor_grasp_msgs::GraspState msg;
    //msg.grasp_state.data = 0;
    //msg.grip.data = 0;
    //grasp_mode_command_pub_.publish(msg);

    flor_grasp_msgs::GraspSelection grasp_msg;
    grasp_msg.header.stamp=ros::Time::now();
    grasp_msg.grasp_id.data      = 0;
    grasp_msg.template_id.data   = 0;
    grasp_msg.template_type.data = 0;
    grasp_release_pub_.publish(grasp_msg);
    //ui->templateButton->setDisabled(true); // unable to move
    //ui->releaseButton->setDisabled(true);
    ui->stitch_template->setDisabled(true);
}

void graspWidget::on_templateButton_clicked()
{
    if(ui->templateBox->count() < 1)
    {
        ROS_ERROR("Tried to template match when no templates exsist");
        return;
    }
    hideHand();
    std::cout << "template match requested..." << std::endl;
    flor_grasp_msgs::TemplateSelection msg;

    int graspID = ui->graspBox->currentText().toInt();
    for(int index = 0; index < grasp_db_.size(); index++)
    {
        if(grasp_db_[index].grasp_id == graspID)
            msg.template_type.data = grasp_db_[index].template_type;
    }
    for(int index = 0; index < template_db_.size(); index++)
    {
        if(template_db_[index].template_type == msg.template_type.data){
            msg.com  = template_db_[index].com;
            msg.mass.data = template_db_[index].mass;
        }
    }
    msg.template_id.data = ui->templateBox->currentIndex();
    msg.pose.pose = last_template_list_.pose[ui->templateBox->currentIndex()].pose;
    msg.pose.header.frame_id = "/world";
    msg.pose.header.stamp = ros::Time::now();
    msg.pose.header.seq++;
    template_match_request_pub_.publish(msg);
}

void graspWidget::on_performButton_clicked()
{
    std::cout << "Performing grasp" << std::endl;
    flor_grasp_msgs::GraspSelection msg;
    msg.header.frame_id = "/world";
    int graspID = ui->graspBox->currentText().toInt();
    msg.grasp_id.data = graspID;
    if(ui->templateBox->count() > 0)
    {
        ui->stitch_template->setDisabled(false);
        on_templateButton_clicked();
        msg.template_id.data = ui->templateBox->currentIndex();
        for(int index = 0; index < grasp_db_.size(); index++)
        {
            if(grasp_db_[index].grasp_id == graspID)
                msg.template_type.data = grasp_db_[index].template_type;
        }
    }
    else
    {
        msg.template_id.data = 0;
        msg.template_type.data = 0;
    }
    grasp_selection_pub_.publish(msg);
    //ui->templateButton->setEnabled(true); // able to move
    //ui->releaseButton->setEnabled(true); // able to release
}

void graspWidget::on_templateBox_activated(const QString &arg1)
{
    // update the selected template id
    QString template_id = ui->templateBox->currentText();
    template_id.remove(template_id.indexOf(": "),template_id.length()-template_id.indexOf(": "));
    selected_template_id_ = template_id.toInt();

    std::cout << "updating the grasp widget grasp selection box contents" << std::endl;
    // clean grasp box
    ui->graspBox->clear();
    selected_grasp_id_ = -1;

    // add grasps to the grasp combo box
    for(int index = 0; index < grasp_db_.size(); index++)
    {
        QString tmp = arg1;
        tmp.remove(0,tmp.indexOf(": ")+2);
        std::cout << "comparing db " << grasp_db_[index].template_name << " to " << tmp.toStdString() << std::endl;

        if(grasp_db_[index].template_name == tmp.toStdString() && grasp_db_[index].hand == hand_)
        {
            std::cout << "Found grasp for template" << std::endl;
            ui->graspBox->addItem(QString::number(grasp_db_[index].grasp_id));
        }
    }

    if(ui->templateBox->count() > 0)
        selected_grasp_id_ = ui->graspBox->itemText(0).toInt();

    if (ui->manualRadio->isChecked())
    {
        ui->graspBox->addItem(QString("CYLINDRICAL"));
        ui->graspBox->addItem(QString("PRISMATIC"));
        ui->graspBox->addItem(QString("SPHERICAL"));
    }
}

void graspWidget::on_graspBox_activated(const QString &arg1)
{
    std::cout << " grasp selection = " << arg1.toStdString() << std::endl;
    if (ui->manualRadio->isChecked())
    {
        flor_grasp_msgs::GraspState msg;
        msg.grasp_state.data = 4; // leave as current command
        if (arg1 == QString("CYLINDRICAL"))  msg.grasp_state.data = 0;
        if (arg1 == QString("PRISMATIC"))    msg.grasp_state.data = 1;
        if (arg1 == QString("SPHERICAL"))    msg.grasp_state.data = 2;
        msg.grasp_state.data += (flor_grasp_msgs::GraspState::MANUAL_GRASP_MODE)<<4;
        msg.grip.data         = ui->userSlider->value();
        msg.finger_effort.resize(FINGER_EFFORTS);
        msg.finger_effort[0].data = ui->verticalSlider->value();
        msg.finger_effort[1].data = ui->verticalSlider_2->value();
        msg.finger_effort[2].data = ui->verticalSlider_3->value();
        msg.finger_effort[3].data = ui->verticalSlider_4->value();
        grasp_mode_command_pub_.publish(msg);
        std::cout << "Sent Manual mode message ("<< uint32_t(msg.grasp_state.data) << ") with " <<  uint32_t(msg.grip.data) << " manual grip level and " <<
                     int8_t(msg.finger_effort[0].data) << "," <<
                     int8_t(msg.finger_effort[1].data) << "," <<
                     int8_t(msg.finger_effort[2].data) << "," <<
                     int8_t(msg.finger_effort[3].data) <<
                     " finger efforts effort to " << hand_ << " hand" << std::endl;
    }
    else
    {
        selected_grasp_id_ = arg1.toInt();
        publishHandPose(arg1.toUInt());
    }
}

void graspWidget::on_noneRadio_clicked()
{
    ui->performButton->setDisabled(true);

    flor_grasp_msgs::GraspState msg;
    msg.grasp_state.data  = (flor_grasp_msgs::GraspState::GRASP_MODE_NONE)<<4;
    msg.grip.data         = 0;

    grasp_mode_command_pub_.publish(msg);
}

void graspWidget::on_templateRadio_clicked()
{
    for (uint32_t ndx=0; ndx < ui->graspBox->count(); ++ndx)
    {
        if ( (ui->graspBox->itemText(ndx) == QString("CYLINDRICAL")) ||
             (ui->graspBox->itemText(ndx) == QString("PRISMATIC")) ||
             (ui->graspBox->itemText(ndx) == QString("SPHERICAL")) ||
             (ui->graspBox->itemText(ndx) == QString("CURRENT")))
        {

            std::string str = ui->graspBox->itemText(ndx).toStdString();
            ROS_INFO(" Removing item at %d (%s)", ndx, str.c_str());
            ui->graspBox->removeItem(ndx);
            --ndx;
        }
    }
    selected_template_id_ = -1;
    if (ui->graspBox->count() < 1)
    {
        ui->graspBox->setDisabled(true); // nothing to select
        ui->performButton->setDisabled(true);
    }
    else
    {
        ui->performButton->setDisabled(false);
    }
    flor_grasp_msgs::GraspState msg;
    msg.grasp_state.data  = (flor_grasp_msgs::GraspState::TEMPLATE_GRASP_MODE)<<4;
    //msg.grip.data         = 0;
    //msg.thumb_effort.data = 0;
    msg.grip.data         = ui->userSlider->value();
    msg.finger_effort.resize(FINGER_EFFORTS);
    msg.finger_effort[0].data = ui->verticalSlider->value();
    msg.finger_effort[1].data = ui->verticalSlider_2->value();
    msg.finger_effort[2].data = ui->verticalSlider_3->value();
    msg.finger_effort[3].data = ui->verticalSlider_4->value();
    grasp_mode_command_pub_.publish(msg);
    std::cout << "Sent Template mode message ("<< uint32_t(msg.grasp_state.data) << ") with " <<  uint32_t(msg.grip.data) << " manual grip level and " <<
                 int8_t(msg.finger_effort[0].data) << "," <<
                 int8_t(msg.finger_effort[1].data) << "," <<
                 int8_t(msg.finger_effort[2].data) << "," <<
                 int8_t(msg.finger_effort[3].data) <<
                 " finger efforts effort to " << hand_ << " hand" << std::endl;

}

void graspWidget::on_manualRadio_clicked()
{
    bool addCylindrical= true;
    bool addPrismatic  = true;
    bool addSpherical  = true;
    bool addCurrent    = true;

    for (uint32_t ndx=0; ndx < ui->graspBox->count(); ++ndx)
    {
        if (ui->graspBox->itemText(ndx) == QString("CYLINDRICAL"))  addCylindrical= false;
        if (ui->graspBox->itemText(ndx) == QString("PRISMATIC"))    addPrismatic  = false;
        if (ui->graspBox->itemText(ndx) == QString("SPHERICAL"))    addSpherical  = false;
        if (ui->graspBox->itemText(ndx) == QString("CURRENT"))      addCurrent    = false;
    }
    if (addCylindrical) ui->graspBox->addItem(QString("CYLINDRICAL"));
    if (addPrismatic)   ui->graspBox->addItem(QString("PRISMATIC"));
    if (addSpherical)   ui->graspBox->addItem(QString("SPHERICAL"));
    if (addCurrent)     ui->graspBox->addItem(QString("CURRENT"));
    ui->graspBox->setDisabled(false);

    ui->performButton->setDisabled(false);

    flor_grasp_msgs::GraspState msg;
    msg.grasp_state.data  = (flor_grasp_msgs::GraspState::MANUAL_GRASP_MODE)<<4;
    msg.grasp_state.data += 4; // no grasp type chosen (force selection) (default to keeping old terminal values)
    msg.grip.data         = ui->userSlider->value();
    msg.finger_effort.resize(FINGER_EFFORTS);
    msg.finger_effort[0].data = ui->verticalSlider->value();
    msg.finger_effort[1].data = ui->verticalSlider_2->value();
    msg.finger_effort[2].data = ui->verticalSlider_3->value();
    msg.finger_effort[3].data = ui->verticalSlider_4->value();
    grasp_mode_command_pub_.publish(msg);
    std::cout << "Sent Manual mode message ("<< uint32_t(msg.grasp_state.data) << ") with " <<  uint32_t(msg.grip.data) << " manual grip level and " <<
                 int8_t(msg.finger_effort[0].data) << "," <<
                 int8_t(msg.finger_effort[1].data) << "," <<
                 int8_t(msg.finger_effort[2].data) << "," <<
                 int8_t(msg.finger_effort[3].data) <<
                 " finger efforts effort to " << hand_ << " hand" << std::endl;
}

void graspWidget::robotStatusCB(const flor_ocs_msgs::OCSRobotStatus::ConstPtr& msg)
{
    uint16_t code;
    uint8_t  severity;
    RobotStatusCodes::codes(msg->status,code,severity);
    //ROS_INFO("  grasp widget code=%d, severity=%d",code,severity);
    ui->robot_status_->setText(robot_status_codes_.str(code).c_str());
}

void graspWidget::templateStitchPoseCallback(const geometry_msgs::PoseStamped::ConstPtr& msg)
{
    if(stitch_template_)
    {
        this->stitch_template_pose_.setRotation(tf::Quaternion(msg->pose.orientation.x,msg->pose.orientation.y,msg->pose.orientation.z,msg->pose.orientation.w));
        this->stitch_template_pose_.setOrigin(tf::Vector3(msg->pose.position.x,msg->pose.position.y,msg->pose.position.z) );
    }
	else
    {
        this->stitch_template_pose_.setIdentity();
    }
}

void graspWidget::handOffsetCallback(const geometry_msgs::PoseStamped::ConstPtr& msg)
{
    this->hand_offset_pose_.setRotation(tf::Quaternion(msg->pose.orientation.x,msg->pose.orientation.y,msg->pose.orientation.z,msg->pose.orientation.w));
    this->hand_offset_pose_.setOrigin(tf::Vector3(msg->pose.position.x,msg->pose.position.y,msg->pose.position.z) );
}

void graspWidget::linkStatesCB( const flor_grasp_msgs::LinkState::ConstPtr& link_states )
{
//    if(hand_type_ == "irobot")
//    {
        double min_feedback = 0, max_feedback = 1.0;
        for(int i = 0; i < link_states->name.size(); i++)
        {
            // get the joint name to figure out the color of the links
            std::string link_name = link_states->name[i].c_str();

            // velocity represents tactile feedback
            double feedback = link_states->tactile_array[i].pressure[0];

            //ROS_ERROR("Applying color to %s",link_name.c_str());
            // calculate color intensity based on min/max feedback
            unsigned char color_intensity = (unsigned char)((feedback - min_feedback)/(max_feedback-min_feedback) * 255.0);
            publishLinkColor(link_name,color_intensity,255-color_intensity,0);
        }
//    }
//    else if(hand_type_ == "robotiq")
//    {
//        double min_feedback = 0, max_feedback = 1.0;
//        for(int i = 0; i < link_states->name.size(); i++)
//        {
//            // get the joint name to figure out the color of the links
//            std::string joint_name = link_states->name[i].c_str();

//            // velocity represents tactile feedback
//            double feedback = link_states->velocity[i];

//            // NOTE: this is SPECIFIC to the irobot hands and how they are setup in the urdf and grasp controllers, IT IS NOT GENERAL
//            std::string link_name = joint_name;

//            //ROS_ERROR("Applying color to %s",link_name.c_str());
//            // calculate color intensity based on min/max feedback
//            unsigned char color_intensity = (unsigned char)((feedback - min_feedback)/(max_feedback-min_feedback) * 255.0);
//            publishLinkColor(link_name,color_intensity,255-color_intensity,0);
//        }
//    }
//    else
//    {
//        //Index 	Name            Link
//        //0         right_f0_j0 	Palm index base
//        //1         right_f0_j1 	Index proximal
//        //2         right_f0_j2 	Index distal
//        //3         right_f1_j0 	Palm middle base
//        //4         right_f1_j1 	Middle proximal
//        //5         right_f1_j2 	Middle distal
//        //6         right_f2_j0 	Palm little base
//        //7         right_f2_j1 	Little proximal
//        //8         right_f2_j2 	Little distal
//        //9         right_f3_j0 	Palm cylinder
//        //10        right_f3_j1 	Thumb proximal
//        //11        right_f3_j2 	Thumb distal

//        double min_feedback = 0, max_feedback = 1.0;
//        for(int i = 0; i < link_states->name.size(); i++)
//        {
//            // get the joint name to figure out the color of the links
//            std::string joint_name = link_states->name[i].c_str();

//            // velocity represents tactile feedback
//            double feedback = link_states->velocity[i];
            
//            // NOTE: this is SPECIFIC to atlas hands, IT IS NOT GENERAL
//            std::string link_name;
//            link_name = joint_name;
//            size_t found = link_name.find('j');
//            if( found != std::string::npos )
//                link_name = link_name.erase(found,1);

//            boost::erase_all(link_name, "/");

//            //ROS_ERROR("Applying color to %s",link_name.c_str());
//            // calculate color intensity based on min/max feedback
//            unsigned char color_intensity = (unsigned char)((feedback - min_feedback)/(max_feedback-min_feedback) * 255.0);
//            publishLinkColor(link_name,color_intensity,255-color_intensity,0);
//        }
//    }
}

void graspWidget::publishLinkColor(std::string link_name, unsigned char r, unsigned char g, unsigned char b)
{
    flor_ocs_msgs::OCSLinkColor cmd;

    cmd.link = link_name;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;

    hand_link_color_pub_.publish(cmd);
}

void graspWidget::publishHandPose(unsigned int id)
{
    //std::cout << "publishing hand pose for grasp id " << id << std::endl;
    //ROS_ERROR("publishing hand pose for grasp id %d",id);
    unsigned int grasp_index;
    for(grasp_index = 0; grasp_index < grasp_db_.size(); grasp_index++)
        if(grasp_db_[grasp_index].grasp_id == id)
            break;

    if(grasp_index == grasp_db_.size()) return;

    // get the selected grasp pose
    geometry_msgs::Pose grasp_transform;//geometry_msgs::PoseStamped grasp_transform;
    if(ui->show_grasp_radio->isChecked())
        grasp_transform = grasp_db_[grasp_index].final_pose;//grasp_transform.pose = grasp_db_[grasp_index].final_pose;
    else
        grasp_transform = grasp_db_[grasp_index].pre_grasp_pose;//grasp_transform.pose = grasp_db_[grasp_index].pre_grasp_pose;

    //ROS_ERROR("Grasp transform before: p=(%f, %f, %f) q=(%f, %f, %f, %f)",grasp_transform.position.x,grasp_transform.position.y,grasp_transform.position.z,grasp_transform.orientation.w,grasp_transform.orientation.x,grasp_transform.orientation.y,grasp_transform.orientation.z);

    // do the necessary transforms for graspit
    staticTransform(grasp_transform);//staticTransform(grasp_transform.pose);

    //ROS_ERROR("Grasp transform after:  p=(%f, %f, %f) q=(%f, %f, %f, %f)",grasp_transform.position.x,grasp_transform.position.y,grasp_transform.position.z,grasp_transform.orientation.w,grasp_transform.orientation.x,grasp_transform.orientation.y,grasp_transform.orientation.z);

    //grasp_transform.header.stamp = ros::Time(0);
    //grasp_transform.header.frame_id = (QString("/template_tf_")+QString::number(selected_template_id_)).toStdString();

    unsigned int template_index;
    for(template_index = 0; template_index < last_template_list_.template_id_list.size(); template_index++)
        if(last_template_list_.template_id_list[template_index] == selected_template_id_)
            break;

    if(template_index == last_template_list_.template_id_list.size()) return;

    geometry_msgs::PoseStamped template_transform;
    template_transform.pose = last_template_list_.pose[template_index].pose;
    //ROS_ERROR("Template transform:     p=(%f, %f, %f) q=(%f, %f, %f, %f)",template_transform.pose.position.x,template_transform.pose.position.y,template_transform.pose.position.z,template_transform.pose.orientation.w,template_transform.pose.orientation.x,template_transform.pose.orientation.y,template_transform.pose.orientation.z);

    geometry_msgs::PoseStamped hand_transform;
    calcWristTarget(grasp_transform, template_transform, hand_transform);
    //ROS_ERROR("Hand transform:         p=(%f, %f, %f) q=(%f, %f, %f, %f)",hand_transform.pose.position.x,hand_transform.pose.position.y,hand_transform.pose.position.z,hand_transform.pose.orientation.w,hand_transform.pose.orientation.x,hand_transform.pose.orientation.y,hand_transform.pose.orientation.z);

    hand_transform.header.stamp = ros::Time::now();
    hand_transform.header.frame_id = "/world";

    // publish
    ghost_hand_pub_.publish(hand_transform);

    virtual_link_joint_states_.position[0] = hand_transform.pose.position.x;
    virtual_link_joint_states_.position[1] = hand_transform.pose.position.y;
    virtual_link_joint_states_.position[2] = hand_transform.pose.position.z;
    virtual_link_joint_states_.position[3] = hand_transform.pose.orientation.x;
    virtual_link_joint_states_.position[4] = hand_transform.pose.orientation.y;
    virtual_link_joint_states_.position[5] = hand_transform.pose.orientation.z;
    virtual_link_joint_states_.position[6] = hand_transform.pose.orientation.w;

    moveit::core::jointStateToRobotState(virtual_link_joint_states_, *hand_robot_state_);

    publishHandJointStates(grasp_index);

    geometry_msgs::PoseStamped planning_hand_target;
    calcPlanningTarget(grasp_transform, template_transform, planning_hand_target);

    planning_hand_target_pub_.publish(planning_hand_target);
}

void graspWidget::publishHandJointStates(unsigned int grasp_index)
{
    sensor_msgs::JointState joint_states;

    joint_states.header.stamp = ros::Time::now();
    joint_states.header.frame_id = std::string("/")+hand_+std::string("_hand_model/")+hand_+"_palm";
    if(hand_type_ == "irobot")
    {

        // must match the order used in the .grasp file
        
        joint_states.name.push_back(hand_+"_f0_j1");
        joint_states.name.push_back(hand_+"_f1_j1");
        joint_states.name.push_back(hand_+"_f2_j1");
        joint_states.name.push_back(hand_+"_f0_j0"); // .grasp finger position [4] -> IGNORE [3], use [4] for both
        joint_states.name.push_back(hand_+"_f1_j0"); // .grasp finger position [4]
        joint_states.name.push_back(hand_+"_f0_j2"); // 0 for now
        joint_states.name.push_back(hand_+"_f1_j2"); // 0 for now
        joint_states.name.push_back(hand_+"_f2_j2"); // 0 for now
        
    }
    else if(hand_type_ == "robotiq")
    {

        // must match the order used in the .grasp file

        joint_states.name.push_back(hand_+"_f0_j1");
        joint_states.name.push_back(hand_+"_f1_j1");
        joint_states.name.push_back(hand_+"_f2_j1");
        joint_states.name.push_back(hand_+"_f1_j0"); // .grasp finger position [4] -> IGNORE [3], use [4] for both
        joint_states.name.push_back(hand_+"_f2_j0"); // .grasp finger position [4]
        joint_states.name.push_back(hand_+"_f0_j2"); // 0 for now
        joint_states.name.push_back(hand_+"_f1_j2"); // 0 for now
        joint_states.name.push_back(hand_+"_f2_j2"); // 0 for now
        joint_states.name.push_back(hand_+"_f0_j3"); // 0 for now
        joint_states.name.push_back(hand_+"_f1_j3"); // 0 for now
        joint_states.name.push_back(hand_+"_f2_j3"); // 0 for now

    }
    else
    {
        // must match those inside of the /sandia_hands/?_hand/joint_states/[right_/left_]+
        joint_states.name.push_back(hand_+"_f0_j0");
        joint_states.name.push_back(hand_+"_f0_j1");
        joint_states.name.push_back(hand_+"_f0_j2");
        joint_states.name.push_back(hand_+"_f1_j0");
        joint_states.name.push_back(hand_+"_f1_j1");
        joint_states.name.push_back(hand_+"_f1_j2");
        joint_states.name.push_back(hand_+"_f2_j0");
        joint_states.name.push_back(hand_+"_f2_j1");
        joint_states.name.push_back(hand_+"_f2_j2");
        joint_states.name.push_back(hand_+"_f3_j0");
        joint_states.name.push_back(hand_+"_f3_j1");
        joint_states.name.push_back(hand_+"_f3_j2");
    }

    joint_states.position.resize(joint_states.name.size());
    joint_states.effort.resize(joint_states.name.size());
    joint_states.velocity.resize(joint_states.name.size());

    for(unsigned int i = 0; i < joint_states.position.size(); ++i)
    {
        joint_states.effort[i] = 0;
        joint_states.velocity[i] = 0;
        if(grasp_index == -1)
            joint_states.position[i] = 0;
        else
            joint_states.position[i] = grasp_db_[grasp_index].finger_joints[i];
        //ROS_ERROR("Setting Finger Joint %s to %f",joint_states.name[i].c_str(),joint_states.position[i]);
    }

    //ghost_hand_joint_state_pub_.publish(joint_states);
    moveit::core::jointStateToRobotState(joint_states, *hand_robot_state_);
    robot_state::robotStateToRobotStateMsg(*hand_robot_state_, display_state_msg_.state);
    robot_state_vis_pub_.publish(display_state_msg_);
}

// assume this function is called within mutex block
int graspWidget::calcWristTarget(const geometry_msgs::Pose& palm_pose, const geometry_msgs::PoseStamped& template_pose, geometry_msgs::PoseStamped& final_pose)
{
    // Transform wrist_pose into the template pose frame
    //   @TODO        "wrist_target_pose.pose   = T(template_pose)*wrist_pose";
    tf::Transform wt_pose;
    tf::Transform tp_pose;
    tf::Transform target_pose;

    wt_pose.setRotation(tf::Quaternion(palm_pose.orientation.x,palm_pose.orientation.y,palm_pose.orientation.z,palm_pose.orientation.w));
    wt_pose.setOrigin(tf::Vector3(palm_pose.position.x,palm_pose.position.y,palm_pose.position.z) );
    tp_pose.setRotation(tf::Quaternion(template_pose.pose.orientation.x,template_pose.pose.orientation.y,template_pose.pose.orientation.z,template_pose.pose.orientation.w));
    tp_pose.setOrigin(tf::Vector3(template_pose.pose.position.x,template_pose.pose.position.y,template_pose.pose.position.z) );

    target_pose = tp_pose * wt_pose * this->hand_offset_pose_ * hand_T_palm_.inverse() * this->stitch_template_pose_ * hand_T_palm_;

    tf::Quaternion tg_quat;
    tf::Vector3    tg_vector;
    tg_quat   = target_pose.getRotation();
    tg_vector = target_pose.getOrigin();

    final_pose.pose.orientation.w = tg_quat.getW();
    final_pose.pose.orientation.x = tg_quat.getX();
    final_pose.pose.orientation.y = tg_quat.getY();
    final_pose.pose.orientation.z = tg_quat.getZ();

    final_pose.pose.position.x = tg_vector.getX();
    final_pose.pose.position.y = tg_vector.getY();
    final_pose.pose.position.z = tg_vector.getZ();
    return 0;
}

void graspWidget::calcPlanningTarget(const geometry_msgs::Pose& palm_pose, const geometry_msgs::PoseStamped& template_pose, geometry_msgs::PoseStamped& planning_hand_pose)
{
  tf::Transform wt_pose;
  tf::Transform tp_pose;

  wt_pose.setRotation(tf::Quaternion(palm_pose.orientation.x,palm_pose.orientation.y,palm_pose.orientation.z,palm_pose.orientation.w));
  wt_pose.setOrigin(tf::Vector3(palm_pose.position.x,palm_pose.position.y,palm_pose.position.z) );
  tp_pose.setRotation(tf::Quaternion(template_pose.pose.orientation.x,template_pose.pose.orientation.y,template_pose.pose.orientation.z,template_pose.pose.orientation.w));
  tp_pose.setOrigin(tf::Vector3(template_pose.pose.position.x,template_pose.pose.position.y,template_pose.pose.position.z) );

  tf::Transform target_pose = tp_pose * wt_pose * hand_T_palm_.inverse();

  tf::Quaternion tg_quat = target_pose.getRotation();
  tf::Vector3    tg_vector = target_pose.getOrigin();

  planning_hand_pose.header.frame_id = "world";
  planning_hand_pose.header.stamp = ros::Time::now();

  planning_hand_pose.pose.orientation.w = tg_quat.getW();
  planning_hand_pose.pose.orientation.x = tg_quat.getX();
  planning_hand_pose.pose.orientation.y = tg_quat.getY();
  planning_hand_pose.pose.orientation.z = tg_quat.getZ();

  planning_hand_pose.pose.position.x = tg_vector.getX();
  planning_hand_pose.pose.position.y = tg_vector.getY();
  planning_hand_pose.pose.position.z = tg_vector.getZ();

}

int graspWidget::staticTransform(geometry_msgs::Pose& palm_pose)
{
    tf::Transform o_T_palm;    //describes palm in object's frame
    tf::Transform o_T_pg;       //describes palm_from_graspit in object's frame

    o_T_pg.setRotation(tf::Quaternion(palm_pose.orientation.x,palm_pose.orientation.y,palm_pose.orientation.z,palm_pose.orientation.w));
    o_T_pg.setOrigin(tf::Vector3(palm_pose.position.x,palm_pose.position.y,palm_pose.position.z) );

    o_T_palm = o_T_pg * gp_T_palm_;

    tf::Quaternion hand_quat;
    tf::Vector3    hand_vector;
    hand_quat   = o_T_palm.getRotation();
    hand_vector = o_T_palm.getOrigin();

    palm_pose.position.x = hand_vector.getX();
    palm_pose.position.y = hand_vector.getY();
    palm_pose.position.z = hand_vector.getZ();
    palm_pose.orientation.x = hand_quat.getX();
    palm_pose.orientation.y = hand_quat.getY();
    palm_pose.orientation.z = hand_quat.getZ();
    palm_pose.orientation.w = hand_quat.getW();

    return 0;
}

int graspWidget::hideHand()
{
    geometry_msgs::PoseStamped hand_transform;
    hand_transform.pose.position.z = 10000;
    hand_transform.pose.orientation.w = 1;
    hand_transform.header.stamp = ros::Time::now();
    hand_transform.header.frame_id = "/world";
    ghost_hand_pub_.publish(hand_transform);

    virtual_link_joint_states_.position[0] = hand_transform.pose.position.x;
    virtual_link_joint_states_.position[1] = hand_transform.pose.position.y;
    virtual_link_joint_states_.position[2] = hand_transform.pose.position.z;
    virtual_link_joint_states_.position[3] = hand_transform.pose.orientation.x;
    virtual_link_joint_states_.position[4] = hand_transform.pose.orientation.y;
    virtual_link_joint_states_.position[5] = hand_transform.pose.orientation.z;
    virtual_link_joint_states_.position[6] = hand_transform.pose.orientation.w;

    moveit::core::jointStateToRobotState(virtual_link_joint_states_, *hand_robot_state_);

    publishHandJointStates(-1);
}

void graspWidget::on_show_grasp_toggled(bool checked)
{
    show_grasp_ = checked;
    ui->show_grasp_radio->setEnabled(show_grasp_);
    ui->show_pre_grasp_radio->setEnabled(show_grasp_);

    robot_state::robotStateToRobotStateMsg(*hand_robot_state_, display_state_msg_.state);
    robot_state_vis_pub_.publish(display_state_msg_);
}

void graspWidget::on_stitch_template_toggled(bool checked)
{
    this->stitch_template_ = checked;
    std::cout << "template stitch requested..." << std::endl;
    flor_grasp_msgs::TemplateSelection msg;
    int graspID = ui->graspBox->currentText().toInt();
    for(int index = 0; index < grasp_db_.size(); index++)
    {
        if(grasp_db_[index].grasp_id == graspID)
            msg.template_type.data = grasp_db_[index].template_type;
    }
    if(checked)
        msg.confidence.data = 1;
    else
        msg.confidence.data = -1;
    msg.pose.header.frame_id = "/world";
    msg.pose.pose = last_template_list_.pose[ui->templateBox->currentIndex()].pose;
    template_stitch_request_pub_.publish(msg);
}

void graspWidget::processObjectSelection(const flor_ocs_msgs::OCSObjectSelection::ConstPtr& msg)
{
    switch(msg->type)
    {
        case flor_ocs_msgs::OCSObjectSelection::TEMPLATE:
            {
            // enable template marker
            //ui->templateBox->setDisabled(false);
            ui->graspBox->setDisabled(false);
            ui->performButton->setDisabled(false);
            ui->stitch_template->setDisabled(false);
            std::vector<unsigned char>::iterator it;
            it = std::find(last_template_list_.template_id_list.begin(), last_template_list_.template_id_list.end(), msg->id);
            if(it != last_template_list_.template_id_list.end())
            {
                int tmp = std::distance(last_template_list_.template_id_list.begin(),it);
                ui->templateBox->setCurrentIndex(tmp);
                on_templateBox_activated(ui->templateBox->itemText(tmp));
                on_templateRadio_clicked();
                selected_template_id_ = tmp;

                if(selected_grasp_id_ != -1 && show_grasp_)
                    publishHandPose(selected_grasp_id_);
            }
            }
            break;
        // not a template
        default:
            {
            selected_template_id_ = -1;
            ui->templateBox->setCurrentIndex(-1);
            ui->graspBox->setCurrentIndex(-1);
            on_templateRadio_clicked();
            //ui->templateBox->setDisabled(true);
            ui->graspBox->setDisabled(true);
            ui->performButton->setDisabled(true);
            ui->stitch_template->setDisabled(true);
            }
            break;
    }
}

void graspWidget::processNewKeyEvent(const flor_ocs_msgs::OCSKeyEvent::ConstPtr &key_event)
{
    // store key state
    if(key_event->state)
        keys_pressed_list_.push_back(key_event->key);
    else
        keys_pressed_list_.erase(std::remove(keys_pressed_list_.begin(), keys_pressed_list_.end(), key_event->key), keys_pressed_list_.end());

    // process hotkeys
    std::vector<int>::iterator key_is_pressed;

    key_is_pressed = std::find(keys_pressed_list_.begin(), keys_pressed_list_.end(), 37);
    /*if(key_event->key == 16 && key_event->state && key_is_pressed != keys_pressed_list_.end()) // ctrl+7
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
    }*/
}

void graspWidget::on_verticalSlider_sliderReleased()
{
    this->on_userSlider_sliderReleased();
}

void graspWidget::on_verticalSlider_2_sliderReleased()
{
    this->on_userSlider_sliderReleased();
}

void graspWidget::on_verticalSlider_3_sliderReleased()
{
    this->on_userSlider_sliderReleased();
}

void graspWidget::on_verticalSlider_4_sliderReleased()
{
    this->on_userSlider_sliderReleased();
}

void graspWidget::on_pushButton_clicked()
{
    ui2 = new handOffsetWidget;

    ui2->show();
}

Ui::graspWidget * graspWidget::getUi()
{
    return ui;
}
QLayout* graspWidget::getMainLayout()
{
    return ui->mainLayout;
}

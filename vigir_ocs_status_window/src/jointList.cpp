#include "jointList.h"
#include <QVBoxLayout>
#include <QRegExp>
#include <ros/ros.h>
#include <urdf/model.h>
#include <QDebug>

jointList::jointList(QWidget *parent) :
   QWidget(parent)
{
   groups = findValidGroups(RobotStateManager::Instance()->getRobotStateSingleton()->getGroups());

   this->setWindowTitle("Joint_Lists");
   this->setMinimumSize(425,120);
   jointTable = new QTreeWidget();
   jointTable->setColumnCount(4);
   QVBoxLayout* main_layout = new QVBoxLayout;

   main_layout->addWidget(jointTable);
   std::cout << "Adding layout..." << std::endl;
   setLayout(main_layout);

   QStringList columns;
   columns.push_back("Joint");
   columns.push_back("Position");
   columns.push_back("Velocity");
   columns.push_back("Effort");

   jointTable->setHeaderLabels(columns);
   jointTable->setColumnWidth(0,150);

   setUpTable();

   joint_states = nh_.subscribe<sensor_msgs::JointState>( "/atlas/joint_states", 2, &jointList::updateList, this );

   key_event_sub_ = nh_.subscribe<flor_ocs_msgs::OCSKeyEvent>( "/flor/ocs/key_event", 5, &jointList::processNewKeyEvent, this );   

}
void jointList::setUpTable()
{
    for(int i = 0; i < groups.size(); i++)
    {
        QTreeWidgetItem* tree = new QTreeWidgetItem(jointTable);
        tree->setText(0, groups[i].name_.c_str());

        for(int m = 0; m < groups[i].joints_.size(); m++)
        {
            QTreeWidgetItem *joint = new QTreeWidgetItem(tree);
            joint->setText(0, groups[i].joints_[m].c_str());
            joints_.push_back(joint);
        }
    }
}

//The groups recieved by the srdf (atleast in atlas) have overlapping groups.  This function picks out the necessary exclusive groups.
//Joint categories in the table are based on groups. ex l_arm_group contains all joints for left arm.
std::vector<srdf::Model::Group> jointList::findValidGroups(std::vector<srdf::Model::Group> groups)
{
    //ex.  group 1 contains joints[A B C]   group 2 contains joints [D E]  group 3 contains joints [A B C D E]
    // only want to return group 1 and group 2 to build jointlist table.

    //stores "bad" groups which contain the joints of multiple groups
    std::map<int, int> badIndexes;

    for(int i = 0; i < groups.size(); i++)
    {
        if(badIndexes.count(i) == 1)//Check if this is a bad group        
            continue;        
        for(int j = i + 1; j < groups.size(); j++)//Compare every group with every other group
        {
            //Note: compilers can be confused by continues and breaks nearby?
            if(badIndexes.count(j) == 1)//Check if this is a bad group
                continue;

            if(badIndexes.count(i) == 1)//this group could be marked bad inside
                break;

            int larger;
            int smaller;
            if(groups[i].joints_.size() > groups[j].joints_.size())//Pick a smaller or larger vector
            {
                larger = i;
                smaller = j;
            }
            else
            {                
                larger = j;
                smaller = i;
            }

            //Compare the two subsets
            bool isSubset = true;
            for(int m = 0; m < groups[smaller].joints_.size(); m++)
            {
                bool hasM = false;
                for(int n = 0; n < groups[larger].joints_.size(); n++)
                {
                    if(groups[smaller].joints_[m].compare(groups[larger].joints_[n]) == 0)
                    {
                        hasM = true;
                        break;
                    }
                }                
                if(!hasM)
                {
                    isSubset = false;
                    break;
                }
            }
            //Check if either list is empty
            if(groups[smaller].joints_.empty())
            {
                badIndexes[smaller] = smaller;
            }
            else if(groups[larger].joints_.empty())
            {
                badIndexes[larger] = larger;
            }
            else if(isSubset)
            {
                badIndexes[larger] = larger;                
            }
        }
    }

    //Create the new group vector with the badIndexes excluded
    std::vector<srdf::Model::Group> updatedGroups;
    for(int i = 0; i < groups.size(); i++)
    {
        if(badIndexes.count(i) == 0)
        {
            updatedGroups.push_back(groups[i]);
        }
    }

    return updatedGroups;
}


jointList::~jointList()
{
   //delete ui;
}
int jointList::getNumError()
{
   return errorCount;
}
int jointList::getNumWarn()
{
   return warnCount;
}

void jointList::updateList(const sensor_msgs::JointState::ConstPtr& states )
{
    //joint states are updated in base3dview only
    MoveItOcsModel* robot_state = RobotStateManager::Instance()->getRobotStateSingleton();

    // clear joint status messages and send Okay state
    Q_EMIT sendJointData(0,"");

    for(int i = 0; i < states->name.size(); i++)
    {

        if (! (i < joints_.size())){
          ROS_ERROR_THROTTLE(10,"Attempt to insert joint index larger than tree widget, aborting! This error is throttled.");
          return;
        }

        //Update the table
        joints_[i]->setText(1,QString::number(states->position.size() > i ? states->position[i] : 0.0));
        joints_[i]->setText(2,QString::number(states->velocity.size() > i ? states->velocity[i] : 0.0));
        joints_[i]->setText(3,QString::number(states->effort.size() > i ? states->effort[i] : 0.0));
        joints_[i]->setBackgroundColor(0,Qt::white);
        joints_[i]->setBackgroundColor(1,Qt::white);
        joints_[i]->setBackgroundColor(3,Qt::white);

        const moveit::core::JointModel* joint =  robot_state->getJointModel(states->name[i]);

        if (!joint){
            ROS_WARN("Searching for joint %s in model returns null pointer! [#2443]", states->name[i].c_str());
            continue;
        }

        //ignore unnecessary joints
        if (joint->getType() == moveit::core::JointModel::PLANAR || joint->getType() == moveit::core::JointModel::FLOATING)
          continue;
        if (joint->getType() == moveit::core::JointModel::REVOLUTE)
          if (static_cast<const moveit::core::RevoluteJointModel*>(joint)->isContinuous())
            continue;
        //calculate joint position percentage relative to max/min limit
        const moveit::core::JointModel::Bounds& bounds = joint->getVariableBounds();
        double distance = bounds[0].max_position_ - bounds[0].min_position_;
        double boundPercent = robot_state->getMinDistanceToPositionBounds(joint) / distance;

        double jointEffortPercent = ((robot_state->getJointEffortLimit(states->name[i]) != 0 && states->effort.size() > i) ?
                                    std::abs(states->effort[i]) / robot_state->getJointEffortLimit(states->name[i]) :
                                    0.0);
        if(jointEffortPercent >=.9 ) //effort error
        {
            Q_EMIT sendJointData(2,QString(joint->getName().c_str()));
            joints_[i]->setBackgroundColor(0,Qt::darkRed);
            joints_[i]->setBackgroundColor(1,Qt::darkRed);
            joints_[i]->parent()->setBackgroundColor(0,Qt::darkRed);
            errorCount++;
        }
        else if(jointEffortPercent >=.75) //effort warn
        {
            Q_EMIT sendJointData(1,QString(joint->getName().c_str()));
            joints_[i]->setBackgroundColor(0,Qt::darkYellow);
            joints_[i]->setBackgroundColor(1,Qt::darkYellow);
            joints_[i]->parent()->setBackgroundColor(0,Qt::darkYellow);
            warnCount++;
        }
        else if(boundPercent <=.03) //position error
        {
            Q_EMIT sendJointData(2,QString(joint->getName().c_str()));
            joints_[i]->setBackgroundColor(0,Qt::red);
            joints_[i]->setBackgroundColor(1,Qt::red);
            joints_[i]->parent()->setBackgroundColor(0,Qt::red);
            errorCount++;
        }
        else if(boundPercent <=.1) //position warn
        {
            Q_EMIT sendJointData(1,QString(joint->getName().c_str()));
            joints_[i]->setBackgroundColor(0,Qt::yellow);
            joints_[i]->setBackgroundColor(1,Qt::yellow);
            joints_[i]->parent()->setBackgroundColor(0,Qt::yellow);
            warnCount++;
        }
    }
}


//?
void jointList::processNewKeyEvent(const flor_ocs_msgs::OCSKeyEvent::ConstPtr &key_event)
{
   // store key state
   if(key_event->state)
       keys_pressed_list_.push_back(key_event->keycode);
   else
       keys_pressed_list_.erase(std::remove(keys_pressed_list_.begin(), keys_pressed_list_.end(), key_event->keycode), keys_pressed_list_.end());

   // process hotkeys
   std::vector<int>::iterator key_is_pressed;

   key_is_pressed = std::find(keys_pressed_list_.begin(), keys_pressed_list_.end(), 37);
   /*if(key_event->keycode == 17 && key_event->state && key_is_pressed != keys_pressed_list_.end()) // ctrl+8
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

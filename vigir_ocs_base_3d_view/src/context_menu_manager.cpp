/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013-2015, TORC Robotics, LLC ( Team ViGIR )
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Team ViGIR, TORC Robotics, nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/
@TODO_ADD_AUTHOR_INFO

#include "context_menu_manager.h"
#include "base_3d_view.h"

ContextMenuManager::ContextMenuManager(vigir_ocs::Base3DView *base_view)
{
    initializing_context_menu_ = 0;    
    base_3d_view_ = base_view;
}

contextMenuItem * ContextMenuManager::addMenuItem(QString name)
{
    contextMenuItem * parent = new contextMenuItem();
    parent->name = name;
    parent->hasChildren = true;
    context_menu_items_.push_back(parent);
    return parent;
}


contextMenuItem *  ContextMenuManager::addActionItem(QString name, boost::function<void()> function, contextMenuItem * parent)
{
    contextMenuItem * child = new contextMenuItem();
    child->name = name;
    child->function = function;
    child->parent = parent;
    child->hasChildren = false;
    context_menu_items_.push_back(child);
    return child;
}

void ContextMenuManager::addCustomItem(contextMenuItem* item)
{
    context_menu_items_.push_back(item);
}

void ContextMenuManager::addSeparatorItem()
{
    contextMenuItem * child = new contextMenuItem();
    child->name = "Separator";
    context_menu_items_.push_back(child);
}

//builds entire context menu including specific widgets
void ContextMenuManager::buildContextMenuHierarchy()
{
    for(int i=0;i<context_menu_items_.size();i++)
    {
        if(context_menu_items_[i]->name.contains("Separator"))
        {
            context_menu_.addSeparator();
            continue;
        }
        //top level menu item
        if(context_menu_items_[i]->parent == NULL)
        {
            if(context_menu_items_[i]->hasChildren)
            {
                QMenu * menu = context_menu_.addMenu(context_menu_items_[i]->name);                
                context_menu_items_[i]->menu = menu;
            }
            else //no children, must be action
            {
                QAction * action = context_menu_.addAction(context_menu_items_[i]->name);             
                context_menu_items_[i]->action = action;
            }
        }
        else // can guarantee parent has already been added provided elements must be added in correct order to vector
        {
            if(context_menu_items_[i]->hasChildren)
            {
                QMenu * menu = context_menu_items_[i]->parent->menu->addMenu(context_menu_items_[i]->name);             
                context_menu_items_[i]->menu = menu;
            }
            else
            {
                QAction * action = context_menu_items_[i]->parent->menu->addAction(context_menu_items_[i]->name);
                context_menu_items_[i]->action = action;
            }
        }
    }
    //update main view ischecked properties if needed
    Q_EMIT updateMainViewItems();
}

void ContextMenuManager::resetMenu()
{
    context_menu_.clear();
    context_menu_.setTitle("Base Menu");
    context_menu_.setStyleSheet("font-size:11px;");    
}

void ContextMenuManager::createContextMenu(bool, int x, int y)
{
    initializing_context_menu_++;

    context_menu_selected_item_ = NULL;

    resetMenu();

    // first we need to query the 3D scene to retrieve the context
    base_3d_view_->emitQueryContext(x,y);

    buildContextMenuHierarchy();

    //toggle visibility of context items for a base view

    //arms selection  only show appropriate arm
    if(base_3d_view_->getActiveContext().find("LeftArm") != std::string::npos)
    {
        setItemVisibility("Select Right Arm",false);

    }
    else if(base_3d_view_->getActiveContext().find("RightArm") != std::string::npos)
    {
        setItemVisibility("Select Left Arm",false);
    }
    else //neither arm selected
    {
        setItemVisibility("Select Right Arm",false);
        setItemVisibility("Select Left Arm",false);
    }

    //footstep goal is still technically a footstep but need seperate case
    if(base_3d_view_->getActiveContext().find("footstep goal") == std::string::npos)
    {
        setItemVisibility("Select Footstep Goal",false);
    }

    //remove footstep-related items if context is not footstep
    if(base_3d_view_->getActiveContext().find("footstep") == std::string::npos || base_3d_view_->getActiveContext().find("footstep goal") != std::string::npos)
    {
        setItemVisibility("Set Starting Footstep",false);
        setItemVisibility("Select Footstep",false);
    }

    //setItemVisibility("Lock Footstep",false);
    //setItemVisibility("Unlock Footstep",false);
    //setItemVisibility("Remove Footstep",false);

    //cannot request footstep plan without goal
    //if(!base_3d_view_->getFootstepVisManager()->hasGoal())
    //{
        //setItemVisibility("Request Step Plan",false);
        //setItemVisibility("Request Step Plan...",false);
    //}

    //set validate visibility based on mode
    if(!base_3d_view_->getFootstepVisManager()->hasGoal())
    {
        setItemVisibility("Send Step Plan Goal to Onboard",false);
        setItemVisibility("Send Step Plan Goal Feet to Onboard",false);
        setItemVisibility("Send Edited Steps to Onboard",false);
        setItemVisibility("Send OCS Step Plan to Onboard",false);

        setItemVisibility("Send Step Plan Goal to OCS Planner", false);
    }
    else
    {                
        if(base_3d_view_->getFootstepVisManager()->getValidateMode() == vigir_ocs_msgs::OCSFootstepSyncStatus::EDITED_STEPS)
        {
            setItemVisibility("Send Step Plan Goal to Onboard",false);
            setItemVisibility("Send Step Plan Goal Feet to Onboard",false);
            //setItemVisibility("Send Edited Steps to Onboard",true);
            setItemVisibility("Send OCS Step Plan to Onboard",false);
        }
        else if(base_3d_view_->getFootstepVisManager()->getValidateMode() == vigir_ocs_msgs::OCSFootstepSyncStatus::CURRENT_PLAN)
        {
            setItemVisibility("Send Step Plan Goal to Onboard",false);
            setItemVisibility("Send Step Plan Goal Feet to Onboard",false);
            setItemVisibility("Send Edited Steps to Onboard",false);
            //setItemVisibility("Send OCS Step Plan to Onboard",true);
        }
        else if(base_3d_view_->getFootstepVisManager()->getValidateMode() == vigir_ocs_msgs::OCSFootstepSyncStatus::GOAL_FEET)
        {
            setItemVisibility("Send Step Plan Goal to Onboard",false);
            //setItemVisibility("Send Step Plan Goal Feet to Onboard",true);
            setItemVisibility("Send Edited Steps to Onboard",false);
            setItemVisibility("Send OCS Step Plan to Onboard",false);
        }
        else if(base_3d_view_->getFootstepVisManager()->getValidateMode() == vigir_ocs_msgs::OCSFootstepSyncStatus::GOAL)
        {
            //setItemVisibility("Send Step Plan Goal to Onboard",true);
            setItemVisibility("Send Step Plan Goal Feet to Onboard",false);
            setItemVisibility("Send Edited Steps to Onboard",false);
            setItemVisibility("Send OCS Step Plan to Onboard",false);
        }

     }

//    if(/*base_3d_view_->getFootstepVisManager()->hasValidStepPlan() || */base_3d_view_->getFootstepVisManager()->numStepPlans() != 1)
//    {
//        //setItemEnabled("Send Step Plan Goal to Onboard",false);

//        setItemEnabled("Send Step Plan Goal Feet to Onboard",false);
//        //setToolTip("Send Step Plan Goal Feet to Onboard","more than one active plan, stitch plans first");
//        setItemEnabled("Send Edited Steps to Onboard",false);
//        //setToolTip("Send Edited Steps to Onboard","more than one active plan, stitch plans first");
//        setItemEnabled("Send OCS Step Plan to Onboard",false);
//        //setToolTip("Send OCS Step Plan to Onboard","more than one active plan, stitch plans first");
//    }
//    else
//    {
//        //setItemEnabled("Send Step Plan Goal to Onboard",true);
//        setItemEnabled("Send Step Plan Goal Feet to Onboard",true);
//        setItemEnabled("Send Edited Steps to Onboard",true);
//        setItemEnabled("Send OCS Step Plan to Onboard",true);
//    }

    if(base_3d_view_->getFootstepVisManager()->numStepPlans() != 1)
    {
        setItemEnabled("Send Current Step Plan",false);
    }
    else
    {
        setItemEnabled("Send Current Step Plan",true);
    }

    //cannot execute without footstep plan
    if(!base_3d_view_->getFootstepVisManager()->hasValidStepPlan())
    {
        setItemVisibility("Execute Step Plan",false);
    }

    // and need to show stitch option if more than one plan
    if(base_3d_view_->getFootstepVisManager()->numStepPlans() < 2)
    {
        setItemVisibility("Stitch Plans",false);
    }

    // update visibility of undo/edo
    if(base_3d_view_->getFootstepVisManager()->hasUndoAvailable() == 0)
    {
        setItemVisibility("Undo Step Change",false);
    }
    if(base_3d_view_->getFootstepVisManager()->hasRedoAvailable() == 0)
    {
        setItemVisibility("Redo Step Change",false);
    }


    if(!base_3d_view_->getFootstepVisManager()->hasStartingFootstep())
    {
        setItemVisibility("Clear Starting Footstep",false);
    }

    // context is stored in the active_context_ variable
    //lock/unlock arms context items
    if(base_3d_view_->getActiveContext().find("template") == std::string::npos)
    {
        //remove context items as not needed
        setItemVisibility("Remove Template",false);
        setItemVisibility("Select Template",false);
        //setItemVisibility("Lock Left Arm to Template",false);
        //setItemVisibility("Lock Right Arm to Template",false);
    }

    //dont' show unless a template is selected as well
    if(base_3d_view_->getSelectedTemplate() == -1)
    {
        setItemVisibility("Lock Left Arm to Template",false);
        setItemVisibility("Lock Right Arm to Template",false);
    }

    if(base_3d_view_->getGhostLeftHandLocked() || base_3d_view_->getGhostRightHandLocked())
    {
        //show only unlock        
        setItemVisibility("Lock Left Arm to Template",false);
        setItemVisibility("Lock Right Arm to Template",false);
    }
    else
    {
        //dont show unlock.. both arms are free and ready to be locked        
        setItemVisibility("Unlock Arms",false);
    }
//    if(flor_atlas_current_mode_ == 0 || flor_atlas_current_mode_ == 100)
//    {
//        executeFootstepPlanMenu->action->setEnabled(true);
//    }
//    else
//    {
//        context_menu_.removeAction(executeFootstepPlanMenu->action);
//    }

    if(base_3d_view_->getCartesianMarkerList().size() == 0)
    {
        //remove cartesian marker menu
        setItemVisibility("Remove All Markers",false);
    }
    else
        setItemVisibility("Remove All Markers",true);


    if(base_3d_view_->getCircularMarker() != NULL)
    {
        setItemVisibility("Create Circular Motion Marker",false);
        setItemVisibility("Remove marker",true);
    }
    else if(base_3d_view_->getCircularMarker() == NULL)
    {
        setItemVisibility("Create Circular Motion Marker",true);
        setItemVisibility("Remove marker",false);
    }

    if(initializing_context_menu_ == 1)
        processContextMenu(x, y);

    initializing_context_menu_--;
}


void ContextMenuManager::processContextMenu(int x, int y)
{
    //tells base3dview to send globalpos   
    QPoint globalPos = base_3d_view_->mapToGlobal(QPoint(x,y));
    context_menu_selected_item_ = context_menu_.exec(globalPos);

    //std::cout << selectedItem << std::endl;
    if(context_menu_selected_item_ != NULL)
    {
        processContextMenuVector(context_menu_selected_item_);
    }
}

void ContextMenuManager::setItemVisibility(QString name, bool visibility)
{
    contextMenuItem* item = NULL;
    //find context menu item in vector
    for(int i=0;i<context_menu_items_.size();i++)
    {
        if(context_menu_items_[i]->name == name)
            item = context_menu_items_[i];
    }
    //can only remove actions?
    if(item != NULL && !item->hasChildren)
        context_menu_.removeAction(item->action);
}

void ContextMenuManager::setItemEnabled(QString name, bool enabled)
{
    contextMenuItem* item = NULL;
    //find context menu item in vector
    for(int i=0;i<context_menu_items_.size();i++)
    {
        if(context_menu_items_[i]->name == name)
            item = context_menu_items_[i];
    }
    //can only remove actions?
    if(item != NULL && !item->hasChildren)
        item->action->setEnabled(enabled);
}


//NOTE::Tooltip does not automatically show, must do something to catch the event and show
void ContextMenuManager::setToolTip(QString name, QString tooltip)
{
    contextMenuItem* item = NULL;
    //find context menu item in vector
    for(int i=0;i<context_menu_items_.size();i++)
    {
        if(context_menu_items_[i]->name == name)
            item = context_menu_items_[i];
    }
    //can only remove actions?
    if(item != NULL )
        item->action->setToolTip(tooltip);

}

//bool ContextMenuManager::event(QEvent * evt)
//{
//    const QHelpEvent *helpEvent = static_cast<QHelpEvent *>(evt);

//    if(evt->type() == QEvent::ToolTip)
//    {
//        //show tooltip
//        QToolTip::showText(helpEvent->globalPos(), context_menu_.activeAction()->toolTip());
//    }
//    else
//    {
//        //propogate
//        return QObject::event(evt);
//    }
//    return true;

//}

void ContextMenuManager::processContextMenuVector(QAction* context_menu_selected_item)
{
    for(int i =0; i<context_menu_items_.size();i++)
    {
        //check parent if it exists?
        if(context_menu_items_[i]->parent != NULL && ((QMenu*)context_menu_selected_item->parent())->title() == context_menu_items_[i]->parent->name )
        {
            //check actual item
            if( context_menu_selected_item->text() == context_menu_items_[i]->name)
            {
                if(context_menu_items_[i]->function != NULL)
                {
                    context_menu_items_[i]->function(); //call binded function
                }
            }
        }
        else // no parent, must still check item
        {
            if( context_menu_selected_item->text() == context_menu_items_[i]->name)
            {
                if(context_menu_items_[i]->function != NULL)
                {
                    context_menu_items_[i]->function(); //call binded function
                }
            }
        }
    }
}




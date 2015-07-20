/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013-2015, Team ViGIR ( TORC Robotics LLC, TU Darmstadt, Virginia Tech, Oregon State University, Cornell University, and Leibniz University Hanover )
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
//@TODO_ADD_AUTHOR_INFO
#include "interactive_marker_server_nodelet.h"

namespace ocs_interactive_marker_server
{
//void InteractiveMarkerServerNodelet::onInit()
InteractiveMarkerServerNodelet::InteractiveMarkerServerNodelet()
{
    interactive_marker_server_feedback_pub_ = nh.advertise<vigir_ocs_msgs::OCSInteractiveMarkerUpdate>( "/flor/ocs/interactive_marker_server/feedback",5, false );
    interactive_marker_server_add_sub_ = nh.subscribe( "/flor/ocs/interactive_marker_server/add", 100, &InteractiveMarkerServerNodelet::addInteractiveMarker, this );
    interactive_marker_server_remove_sub_ = nh.subscribe( "/flor/ocs/interactive_marker_server/remove", 100, &InteractiveMarkerServerNodelet::removeInteractiveMarker, this );
    interactive_marker_server_update_sub_ = nh.subscribe( "/flor/ocs/interactive_marker_server/update", 100, &InteractiveMarkerServerNodelet::updatePose, this);
    interactive_marker_server_mode_sub_ = nh.subscribe( "/flor/ocs/control_modes", 5, &InteractiveMarkerServerNodelet::setMode, this );
    //interactive_marker_server_visibility_sub_ = nh.subscribe("/flor/ocs/interactive_marker_server/visibility",5, &InteractiveMarkerServerNodelet::processMarkerVisibility,this);

    // related to object selection
    select_object_sub_ = nh.subscribe( "/flor/ocs/object_selection", 5, &InteractiveMarkerServerNodelet::processObjectSelection, this );
    selected_object_update_pub_ = nh.advertise<vigir_ocs_msgs::OCSSelectedObjectUpdate>( "/flor/ocs/interactive_marker_server/selected_object_update", 5, false);

    //marker_feedback_timer_ = boost::posix_time::microsec_clock::universal_time();
}

void InteractiveMarkerServerNodelet::publishSelectedObject()
{
    //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_change_mutex_ );

    for (std::map<std::string,std::string>::iterator it = host_selected_object_topic_map_.begin(); it != host_selected_object_topic_map_.end(); ++it)
    {
        std::string host = it->first;
        std::string selected_object_topic = it->second;

        vigir_ocs_msgs::OCSSelectedObjectUpdate cmd;
        cmd.topic = selected_object_topic;
        cmd.pose = pose_map_[selected_object_topic];
        cmd.host = host;
        selected_object_update_pub_.publish(cmd);
    }
}

void InteractiveMarkerServerNodelet::addInteractiveMarker(const vigir_ocs_msgs::OCSInteractiveMarkerAdd::ConstPtr msg)
{
    //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_change_mutex_ );

    // name, topic, frame, scale, point
    if (marker_map_.find(msg->topic) == marker_map_.end())
    {
        ROS_INFO("Adding marker %s", msg->topic.c_str());

        marker_map_[msg->topic].reset(new InteractiveMarkerServerCustom(msg->name, msg->topic, msg->mode, msg->frame, msg->scale, msg->point));
        marker_map_[msg->topic]->onFeedback = boost::bind(&InteractiveMarkerServerNodelet::onMarkerFeedback, this, _1, _2, _3, _4);
        geometry_msgs::PoseStamped pose;
        pose.header.frame_id = "/world";
        pose.header.stamp = ros::Time::now();
        pose.pose.position.x = msg->point.x;
        pose.pose.position.y = msg->point.y;
        pose.pose.position.z = msg->point.z;
        pose.pose.orientation.w = msg->orientation.w;
        pose.pose.orientation.x = msg->orientation.x;
        pose.pose.orientation.y = msg->orientation.y;
        pose.pose.orientation.z = msg->orientation.z;
        marker_map_[msg->topic]->setPose(pose);
    }

}

void InteractiveMarkerServerNodelet::removeInteractiveMarker( const std_msgs::String::ConstPtr msg )
{
    //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_change_mutex_ );

    //ROS_ERROR("%s marker exists?", msg->data.c_str());
    if(marker_map_.find(msg->data) != marker_map_.end())
    {
        //marker_map_[msg->data];
        //ROS_INFO("Removing marker %s", msg->data.c_str());
        //delete marker_map_[msg->data];
        marker_map_.erase(marker_map_.find(msg->data));
    }
}

void InteractiveMarkerServerNodelet::updatePose( const vigir_ocs_msgs::OCSInteractiveMarkerUpdate::ConstPtr msg )
{
    //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_change_mutex_ );

    if(marker_map_.find(msg->topic) != marker_map_.end())
    {
        // return if message is older than latest change
        //if(msg->pose.header.stamp < pose_map_[msg->topic].header.stamp)
        //    return;

        // simply sets the pose of the marker
        marker_map_[msg->topic]->setPose(msg->pose);

        // close the loop by sending feedback IF needed
        if(msg->update_mode == vigir_ocs_msgs::OCSInteractiveMarkerUpdate::SET_POSE)
        {
            marker_map_[msg->topic]->onFeedback(msg->event_type,msg->topic,msg->pose,msg->client_id);
            pose_map_[msg->topic] = msg->pose;
            publishSelectedObject();
        }
    }
}

void InteractiveMarkerServerNodelet::onMarkerFeedback(unsigned char event_type, std::string topic_name, geometry_msgs::PoseStamped pose, std::string client_id)
{
    // multithreaded application, multiple threads sending this, so would need a timer per thread
//    boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
//    boost::posix_time::time_duration diff = now - marker_feedback_timer_;

//    ROS_ERROR("%f",static_cast<float>(diff.total_milliseconds()));
//    // throttle marker feedback to ~30hz
//    if(event_type == visualization_msgs::InteractiveMarkerFeedback::POSE_UPDATE && static_cast<float>(diff.total_milliseconds()) < 33.333f)
//        return;

//    ROS_ERROR("send");

//    marker_feedback_timer_ = boost::posix_time::microsec_clock::universal_time();

    
    vigir_ocs_msgs::OCSInteractiveMarkerUpdate cmd;
    cmd.client_id = client_id;
    cmd.topic = topic_name;
    cmd.pose = pose;
    cmd.pose.header.stamp = ros::Time::now();
    cmd.event_type = event_type;
    //{
    //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_publisher_mutex_ );
    interactive_marker_server_feedback_pub_.publish(cmd);
    //}

    if(event_type == visualization_msgs::InteractiveMarkerFeedback::MOUSE_UP)
    {

        //changing mode sets up marker for responsiveness, "fix"
        std::map<std::string,boost::shared_ptr<InteractiveMarkerServerCustom> >::iterator iter;
        for (iter = marker_map_.begin(); iter != marker_map_.end(); ++iter)
        {
            iter->second->setMode(iter->second->getMode());
        }

        publishSelectedObject();
    }
}

void InteractiveMarkerServerNodelet::setMode(const vigir_ocs_msgs::OCSControlMode::ConstPtr msg)
{
    //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_change_mutex_ );

    //ROS_ERROR("CHANGING MODE");
    std::map<std::string,boost::shared_ptr<InteractiveMarkerServerCustom> >::iterator iter;
    for (iter = marker_map_.begin(); iter != marker_map_.end(); ++iter)
        if(iter->second->getMode() != vigir_ocs_msgs::OCSInteractiveMarkerAdd::WAYPOINT_3DOF)
            iter->second->setMode(msg->manipulationMode);
}

//void InteractiveMarkerServerNodelet::processMarkerVisibility(const vigir_ocs_msgs::OCSMarkerVisibility::ConstPtr msg)
//{
//    //ROS_ERROR("marker visibiilty");
//    //set visibility of different interactive markers
//    if(msg->all_markers)
//    {
//        //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_change_mutex_ );

//        std::map<std::string,InteractiveMarkerServerCustom*>::iterator iter;
//        for (iter = marker_map_.begin(); iter != marker_map_.end(); ++iter)
//        {
//            iter->second->setVisible(msg->all_markers_visibility);
//        }
//    }
//    //TODO:: change markers to have visibility set via changing markers, not enable/disable
//    //not worrying about selective visibility of markers for now
////    else // just disable by type
////    {
////        std::map<std::string,InteractiveMarkerServerCustom*>::iterator iter;
////        for (iter = marker_map_.begin(); iter != marker_map_.end(); ++iter)
////        {
////            //string comparison on the marker's topic to determine type
////            std::string topic_name = iter->first;
////            ROS_ERROR("marker topic %s",topic_name.c_str());
////            //templates
////            if(topic_name.find("/template"))
////            {
////                iter->second->setVisible(msg->template_visibility);
////            }
////            else if(topic_name.find("/footstep_"))
////            {
////                iter->second->setVisible(msg->template_visibility);
////            }
////            else if(topic_name.find("/footstep_goal_"))
////            {
////                iter->second->setVisible(msg->template_visibility);
////            }
////        }
////    }
//}

void InteractiveMarkerServerNodelet::processObjectSelection(const vigir_ocs_msgs::OCSObjectSelection::ConstPtr msg)
{
    //boost::recursive_mutex::scoped_lock lock( interactive_marker_server_change_mutex_ );

    // save the interactive marker topic
    std::string selected_object_topic = "";

    try
    {
        //Get id of object that is selected
        switch(msg->type)
        {
            case vigir_ocs_msgs::OCSObjectSelection::TEMPLATE:
                selected_object_topic = "/template_"+boost::lexical_cast<std::string>(msg->id)+"_marker";
                break;
            case vigir_ocs_msgs::OCSObjectSelection::FOOTSTEP:
                selected_object_topic = "/footstep_"+boost::lexical_cast<std::string>(msg->id/2)+"_marker";
                break;
            case vigir_ocs_msgs::OCSObjectSelection::FOOTSTEP_GOAL:
                selected_object_topic = "/footstep_goal_"+boost::lexical_cast<std::string>(msg->id/2 ? "right" : "left")+"_marker";
                break;
            case vigir_ocs_msgs::OCSObjectSelection::END_EFFECTOR:
                selected_object_topic = (msg->id == vigir_ocs_msgs::OCSObjectSelection::LEFT_ARM ? "/l_arm_pose_marker" : "/r_arm_pose_marker");
                break;
            default:
                break;
        }

        ROS_INFO("SELECTED OBJECT: %s", selected_object_topic.c_str());
    }
    catch(...)
    {
        ROS_ERROR("Error parsing selected object name...");
    }

    try
    {
        if(marker_map_.find(selected_object_topic) != marker_map_.end())
        {
            host_selected_object_topic_map_[msg->host] = selected_object_topic;
            pose_map_[host_selected_object_topic_map_[msg->host]] = marker_map_[selected_object_topic]->getPose();

            publishSelectedObject();
        }
    }
    catch(...)
    {
        ROS_ERROR("Something went wrong with object selection...");
    }
}

}

//PLUGINLIB_DECLARE_CLASS (vigir_ocs_interactive_marker_server_nodelet, InteractiveMarkerServerNodelet, ocs_interactive_marker_server::InteractiveMarkerServerNodelet, nodelet::Nodelet);

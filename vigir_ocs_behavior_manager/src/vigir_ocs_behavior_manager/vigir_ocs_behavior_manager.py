#!/usr/bin/env python

import roslib; roslib.load_manifest('vigir_ocs_behavior_manager')
import rospy
import pickle
import actionlib

from vigir_be_input.complex_action_server import ComplexActionServer

from vigir_be_msgs.msg import BehaviorInputAction , BehaviorInputFeedback, BehaviorInputResult, BehaviorInputGoal
#from python_qt_binding.QtCore import Slot, Signal
#from python_qt_binding.QtGui import QWidget, QHBoxLayout, QVBoxLayout, QLabel, QDoubleSpinBox, QCheckBox
from geometry_msgs.msg import Point, PoseStamped#, JointState
from flor_ocs_msgs.msg import OCSObjectSelection, OCSTemplateList
from vigir_footstep_planning_msgs.msg import StepPlan
from std_msgs import Header

'''
Created on 02/26/2015

@author: Brian Wright
'''

class BehaviorManager():

    def __init__(self):
        '''
        Constructor
        '''

        #sub to grab latest data that may be required
        self.point_sub = rospy.Subscriber('/new_point_cloud_request', Point, self.point_cloud_cb)
        self.object_sub = rospy.Subscriber('/flor/ocs/object_selection', OCSObjectSelection, self.object_cb)
        self.template_sub = rospy.Subscriber('/template/list', OCSTemplateList,self.template_cb)
        self.goal_waypoint_sub = rospy.Subscriber('/flor/ocs/footstep/goal_pose',PoseStamped,self.goal_waypoint_cb)
        self.footstep_plan_sub = rospy.Subscriber('/vigir/footstep_manager/current_step_plan',StepPlan ,self.footstep_plan_cb)
        #self.ghost_joint_state_sub = rospy.Subscriber('/flor/ghost/get_joint_states',JointState, self.ghost_joint_state_cb)

         #"/flor/ghost/pose/left_hand"
         #"/flor/ghost/pose/right_hand"
         #"/flor/ghost/pose/robot"


        #server to communicate with Behavior system and send serialized data
        self.serial_server_ = ComplexActionServer('/vigir/ocs/behavior_ocs', BehaviorInputAction, execute_cb=self.receive_behavior_cb, auto_start = False)
        self.serial_server_.start()

        #variables to store latest published data
        self.latest_3d_point_ = Point()
        self.latest_selected_template_ = OCSObjectSelection()
        self.template_list_ = OCSTemplateList()
        self.latest_goal_waypoint_pose_  = PoseStamped()
        self.latest_footstep_plan_ = StepPlan()
        #self.ghost_joint_state = JointState()


    def receive_behavior_cb(self,goal, goal_handle):
        print 'received'
        #take goal, send ui wait for the behavior to be completed
        #client to communicate with relay and create notifications in ui
        relay_client_ = actionlib.SimpleActionClient('/vigir/ocs/behavior_relay_ui',BehaviorInputAction)
        relay_client_.wait_for_server()
        #send to relay
        relay_client_.send_goal(goal)
        relay_client_.wait_for_result()
        print 'got result'
        #get result and reset pickle
        result = BehaviorInputResult()
        result = relay_client_.get_result()

        #dont grab data if aborted
        if(result.result_code == BehaviorInputResult.RESULT_ABORTED):
            self.serial_server_.set_succeeded(BehaviorInputResult(result_code=result.result_code, data="Aborted"),"Aborted",goal_handle)
            return

        #get data for result based on msg
        if(goal.request_type == BehaviorInputGoal.POINT_LOCATION):
            result.data = self.latest_3d_point_
        #only handling templates
        elif(goal.request_type == BehaviorInputGoal.SELECTED_OBJECT_ID):
            #dont return message, just index
            result.data = self.latest_selected_template_.id
        elif(goal.request_type == BehaviorInputGoal.WAYPOINT_GOAL_POSE):
            result.data = self.latest_goal_waypoint_pose_
        elif(goal.request_type == BehaviorInputGoal.FOOTSTEP_PLAN_HEADER):
            result.data = self.latest_footstep_plan_.header
        else:
            print 'behavior not yet implemented'
            result.data = 'behavior not yet implemented'

        #serialize with pickle
        data_msg = result.data
        data_str = pickle.dumps(data_msg)

        self.serial_server_.set_succeeded(BehaviorInputResult(result_code=result.result_code, data=data_str),"ok",goal_handle)


    #------------callbacks to grab data from OCS------------------#

    def point_cloud_cb(self,data):
        self.latest_3d_point_ = data

    def object_cb(self,data):
        #only store for latest template selected
        if(data.type == OCSObjectSelection.TEMPLATE):
            self.latest_selected_template_ = data

    def template_cb(self,data):
        #use selected object to grab pose in list?
        self.template_list_ = data

    def goal_waypoint_cb(self,data):
        self.latest_goal_waypoint_pose_ = data

    def footstep_plan_cb(self,data):
        self.latest_footstep_plan_ = data
                
    #def ghost_joint_state_cb(self,data):
            #self.ghost_joint_state = data

    #------------end callbacks to grab data from OCS--------------#


















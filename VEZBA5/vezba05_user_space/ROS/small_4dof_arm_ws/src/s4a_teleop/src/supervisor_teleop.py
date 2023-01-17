#!/usr/bin/python

import rospy

from sensor_msgs.msg import Joy
from std_msgs.msg import Bool

import moveit_commander

import subprocess

def fill(fill_with, N):
	v = []
	for i in range(N):
		v.append(fill_with)
	return v

class SupervisorTeleop:
	def __init__(self):
		self.first_time = True
		
		self.joy_sub = rospy.Subscriber("joy", Joy, self.on_joy)
		
		self.proceed_re_pub = rospy.Publisher(
			'proceed_with_vision_re',
			Bool,
			queue_size = 1
		)
		self.proceed_hold_pub = rospy.Publisher(
			'proceed_with_vision_hold',
			Bool,
			queue_size = 1
		)
		
		rospy.loginfo('''

Supervisor Teleop:
	
	Buttons:
		3: Trigger proceeding of one vision servo cmd for short period of time
		4: Proceed with vision servo cmds while holding
		
		''')

	def on_joy(self, data):
		if self.first_time:
			self.first_time = False
			self.prev_buttons = data.buttons
			self.buttons_re = fill(False, len(data.buttons))
			return
		
		for i in range(len(self.buttons_re)):
			self.buttons_re[i] = not self.prev_buttons[i] and data.buttons[i]
		self.prev_buttons = data.buttons

			
		msg = Bool()
		msg.data = self.buttons_re[2] # 3
		self.proceed_re_pub.publish(msg)
		
		
		msg = Bool()
		msg.data = data.buttons[3] # 4
		self.proceed_hold_pub.publish(msg)
		

if __name__ == '__main__':
	rospy.init_node('supervisor_teleop')
	controller = SupervisorTeleop()
	rospy.spin()

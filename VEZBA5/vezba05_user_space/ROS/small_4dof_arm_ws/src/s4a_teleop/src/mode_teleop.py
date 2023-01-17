#!/usr/bin/python

import rospy

from sensor_msgs.msg import Joy
from std_msgs.msg import Bool

import subprocess

modes = ['jog', 'servo']

def fill(fill_with, N):
	v = []
	for i in range(N):
		v.append(fill_with)
	return v

class ModeTeleop:
	def __init__(self):
		self.first_time = True
		
		self.mode = modes[0]
		self.motors_en = False
		
		self.joy_sub = rospy.Subscriber("joy", Joy, self.on_joy)
		
		self.motors_en_pub = rospy.Publisher(
			'motors_en',
			Bool,
			queue_size = 1
		)
		
		rospy.loginfo('''

Mode Teleop:
	
	Buttons:
		Select: change mode
		Start: Start/Stop motors
		
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
		
		
		if self.buttons_re[8]: # Select
			i = modes.index(self.mode) + 1
			assert(i <= len(modes))
			if i == len(modes):
				i = 0
			self.mode = modes[i]
			rospy.loginfo('Current mode: {}'.format(self.mode))
			if self.mode == 'jog':
				ctrl = 'jog'
			elif self.mode == 'servo':
				ctrl = 'servo'
				#ctrl = 'traj_servo'
			cmd = 'rosrun s4a_teleop change_controller.py --silent'.split()
			cmd.append(ctrl)
			subprocess.run(cmd)
			
			
		if self.buttons_re[9]: # Start
			self.motors_en = not self.motors_en
			if self.motors_en:
				rospy.loginfo('Motors enabled')
			else:
				rospy.loginfo('Motors disabled')
			msg = Bool()
			msg.data = self.motors_en
			self.motors_en_pub.publish(msg)
		
		

if __name__ == '__main__':
	rospy.init_node('mode_teleop')
	controller = ModeTeleop()
	rospy.spin()

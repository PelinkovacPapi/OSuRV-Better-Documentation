#!/usr/bin/python

import rospy
import rospkg

from std_msgs.msg import String

from moveit_commander import (
	MoveGroupCommandInterpreter,
	MoveGroupInfoLevel,
)

import os
import subprocess


class bcolors:
	HEADER = "\033[95m"
	OKBLUE = "\033[94m"
	OKGREEN = "\033[92m"
	WARNING = "\033[93m"
	FAIL = "\033[91m"
	ENDC = "\033[0m"
	
def print_message(level, msg):
	if level == MoveGroupInfoLevel.FAIL:
		print(bcolors.FAIL + msg + bcolors.ENDC)
	elif level == MoveGroupInfoLevel.WARN:
		print(bcolors.WARNING + msg + bcolors.ENDC)
	elif level == MoveGroupInfoLevel.SUCCESS:
		print(bcolors.OKGREEN + msg + bcolors.ENDC)
	elif level == MoveGroupInfoLevel.DEBUG:
		print(bcolors.OKBLUE + msg + bcolors.ENDC)
	else:
		print(msg)


class RoutinesServer:
	def __init__(self):
		rospack = rospkg.RosPack()
		pkg_dir = rospack.get_path('s4a_teleop')
		self.routines_dir = os.path.join(pkg_dir, 'routines/')
		
		self.run_routine_sub = rospy.Subscriber(
			"run_routine",
			String,
			self.on_run_routine
		)
		
	
	'''
		Recursive function
	'''
	def exec_routine_script(self, routine_basename):
		fn = os.path.join(self.routines_dir, routine_basename)
		with open(fn) as f:
			for line in f.readlines():
				if line.strip() == '':
					# Skip empty line.
					continue
				elif line.lstrip().startswith('#'):
					# Skip comment.
					continue
				elif line.lstrip().startswith('%include'):
					# Include recusively routine.
					bn2 = line.strip().split()[1]
					self.exec_routine_script(bn2)
				else:
					# Input.
					name = ''
					ag = self.cmd_interp.get_active_group()
					if ag != None:
						name = ag.get_name()
					print(
						bcolors.OKBLUE + name + '> ' + bcolors.ENDC + line,
						end = ''
					)
					
					# Execute.
					(level, msg) = self.cmd_interp.execute(line)
					
					# Output.
					print_message(level, msg)
	
	
	def go_routine_mode_and_exec(self, routine_name):
		rospy.loginfo('Running routine: {}'.format(routine_name))
		
		# Change mode.
		def change_mode(mode):
			cmd = 'rosrun s4a_teleop change_controller.py --silent'.split()
			cmd.append(mode)
			subprocess.run(cmd)
		
		
		change_mode('traj')
		
		self.cmd_interp = MoveGroupCommandInterpreter()
		
		self.exec_routine_script(routine_name + '.moveitcmd')

		# Return back.
		change_mode('servo')


	def on_run_routine(self, msg):
		routine_name = msg.data
		self.go_routine_mode_and_exec(routine_name)
		
		

if __name__ == '__main__':
	rospy.init_node('routines_server')
	controller = RoutinesServer()
	rospy.spin()

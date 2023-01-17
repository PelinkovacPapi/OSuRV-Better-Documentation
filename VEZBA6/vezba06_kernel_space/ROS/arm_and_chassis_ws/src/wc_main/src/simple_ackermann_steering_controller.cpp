
#include "simple_ackermann_steering_controller.hpp"

// Config.
#define LOOP_HZ 25

#include "motor_ctrl.h"


// Includes for driver.
#include <string.h> // strerror()
#include <unistd.h> // file ops
#include <fcntl.h> // open() flags
#include <sys/ioctl.h> // ioctl()


#include <algorithm>
using namespace std;

#include <angles/angles.h>


#define DEBUG(x) do{ ROS_DEBUG_STREAM(#x << " = " << x); }while(0)

template<typename T>
static T sym_clamp(T x, T limit) {
	return clamp(x, -limit, limit);
}

SimpleAckermannSteeringController::SimpleAckermannSteeringController(ros::NodeHandle& nh) :
	nh(nh),
	all_motors_sim(false),
	drv_fd(-1),
	motors_en(true)
{
	
	
	ros::param::param("~all_motors_sim", all_motors_sim, false);
	ROS_INFO(
		"all_motors_sim = %s",
		all_motors_sim ? "true" : "false"
	);
	
	
	if(!all_motors_sim){
		// Open driver.
		drv_fd = open(DEV_FN, O_RDWR);
		if(drv_fd < 0){
			ROS_WARN(
				"\"%s\" not opened! drv_fd = %d -> %s",
				DEV_FN, drv_fd, strerror(-drv_fd)
			);
		}
		
		motor_ctrl__ioctl_arg_moduo_t ia;
		ia.ch = 0;
		ia.moduo = 20; // 5kHz
		int r = ioctl(
			drv_fd,
			IOCTL_MOTOR_CLTR_SET_MODUO,
			*(unsigned long*)&ia
		);
		if(r){
			ROS_WARN("ioctl went wrong!");
		}
	}
	
	ros::Duration timer_period = ros::Duration(1.0/LOOP_HZ);
	non_realtime_loop_timer = nh.createTimer(
		timer_period,
		&SimpleAckermannSteeringController::publish_odom,
		this
	);
	
	motors_en_sub = nh.subscribe(
		"motors_en",
		1,
		&SimpleAckermannSteeringController::motors_en_cb,
		this
	);
	
	cmd_sub = nh.subscribe(
		"cmd",
		1,
		&SimpleAckermannSteeringController::cmd_cb,
		this
	);
	
	odom_pub  = nh.advertise<nav_msgs::Odometry>(
		"odom",
		1
	);
}

SimpleAckermannSteeringController::~SimpleAckermannSteeringController() {
	if(!all_motors_sim){
		// Close driver.
		ROS_INFO("closing drv_fd");
		close(drv_fd);
	}
}

void SimpleAckermannSteeringController::publish_odom(const ros::TimerEvent& e) {
/*
	ros::Time now = ros::Time::now();
	
	if(all_motors_sim){
		motor_ctrl__read_arg_fb_t ra;
		r = read(drv_fd, (char*)&ra, sizeof(ra));
		if(r != sizeof(ra)){
			ROS_WARN("read went wrong!");
		}
		
		int64_t pulse_cnt_fb = ra.pulse_cnt_fb[0];
		printf("pulse_cnt_fb = %lld\n", pulse_cnt_fb);
	}else{
		
	}
	
	odom_pub.publish(odom_msg);
	
	
*/
}

void SimpleAckermannSteeringController::motors_en_cb(
	const std_msgs::Bool::ConstPtr& msg
) {
	motors_en = msg->data;
	ROS_INFO(
		"Motors %s", motors_en ? "enabled" : "disabled"
	);
}


void SimpleAckermannSteeringController::cmd_cb(
	const ackermann_msgs::AckermannDriveStamped::ConstPtr& msg
) {
	//TODO Limits roc:
	// steering_angle_velocity
	// acceleration
	speed = msg->drive.speed;
	float steering_angle = msg->drive.steering_angle;
	
	if(all_motors_sim){
#if 1
		ROS_DEBUG("speed = %f", speed);
		ROS_DEBUG("steering_angle = %f", steering_angle);
#endif
	}
	
	if(!all_motors_sim && motors_en){
		int16_t duty[2];
		
		// |speed| = [0, 100] -> threshold = [10, 0].
		// it will be <<1 in write() so then will be [20, 0].
		// duty will also calculate in direction.
		uint8_t abs_speed;
		uint16_t threshold;
		if(speed > 0){
			abs_speed = speed;
			threshold = (100-abs_speed)/10;
			duty[0] = threshold;
		}else{
			abs_speed = -speed;
			threshold = (100-abs_speed)/10;
			duty[0] = -threshold;
		}
		
		float sa = -steering_angle; // + is left.
		// -pi/2=2.5%=25 +pi/2=12.5%=125
		duty[1] = (sa + M_PI_2)*(100/M_PI) + 25;
		
		int r = write(drv_fd, (char*)&duty, sizeof(duty));
		if(r != sizeof(duty)){
			ROS_WARN("write went wrong!");
		}
	}
}

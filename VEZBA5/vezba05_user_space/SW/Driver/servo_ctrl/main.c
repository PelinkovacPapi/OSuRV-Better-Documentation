
#include <linux/module.h> // module_init(), module_exit()
#include <linux/fs.h> // file_operations
#include <linux/errno.h> // EFAULT
#include <linux/uaccess.h> // copy_from_user(), copy_to_user()

MODULE_LICENSE("Dual BSD/GPL");

#include "gpio.h"
#include "hw_pwm.h"
#include "sw_pwm.h"

#define N_SERVOS (HW_PWM__N_CH+SW_PWM__N_CH)


#define DEV_NAME "servo_ctrl"



static int servo_ctrl_open(struct inode *inode, struct file *filp) {
	return 0;
}

static int servo_ctrl_release(struct inode *inode, struct file *filp) {
	return 0;
}

static uint16_t cmd_duty_permille[N_SERVOS];

static ssize_t servo_ctrl_write(struct file* filp, const char *buf, size_t len, loff_t *f_pos) {
	int i;
	uint32_t dp;
	
	if (copy_from_user(cmd_duty_permille, buf, len) != 0) {
		return -EFAULT;
	}else{
		for(i = 0; i < N_SERVOS; i++){
			// Protection.
			dp = cmd_duty_permille[i];
			if(dp > 1000){
				dp = 1000;
			}
			// Shift for 1 because 2000 is period.
			if(i < HW_PWM__N_CH){
				hw_pwm__set_threshold(i, dp << 1);
			}else{
				sw_pwm__set_threshold(i-HW_PWM__N_CH, dp << 1);
			}
		}
		return len;
	}
}


static ssize_t servo_ctrl_read(struct file* filp, char* buf, size_t len, loff_t* f_pos){
	if(copy_to_user(buf, cmd_duty_permille, len) != 0){
		return -EFAULT;
	}else{
		return len;
	}
}


static struct file_operations servo_ctrl_fops = {
	open    : servo_ctrl_open,
	release : servo_ctrl_release,
	read    : servo_ctrl_read,
	write   : servo_ctrl_write
};


int servo_ctrl_init(void) {
	int result;
	int i;

	printk(KERN_INFO DEV_NAME": Inserting module...\n");

	result = register_chrdev(DEV_MAJOR, DEV_NAME, &servo_ctrl_fops);
	if(result < 0){
		printk(KERN_ERR DEV_NAME": cannot obtain major number %d!\n", DEV_MAJOR);
		return result;
	}

	result = gpio__init();
	if(result){
		printk(KERN_ERR DEV_NAME": gpio__init() failed!\n");
		goto gpio_init__fail;
	}

	result = hw_pwm__init();
	if(result){
		printk(KERN_ERR DEV_NAME": hw_pwm__init() failed!\n");
		goto hw_pwm__init__fail;
	}
	
	result = sw_pwm__init();
	if(result){
		printk(KERN_ERR DEV_NAME": sw_pwm__init() failed!\n");
		goto sw_pwm__init__fail;
	}
	
	
	// 10us*2000 -> 20ms.
	for(i = 0; i < HW_PWM__N_CH; i++){
		hw_pwm__set_moduo(i, 1000 << 1);
		hw_pwm__set_threshold(i, 75 << 1);
	}
	for(i = 0; i < SW_PWM__N_CH; i++){
		sw_pwm__set_moduo(i, 1000 << 1);
		sw_pwm__set_threshold(i, 75 << 1);
	}
	
	printk(KERN_INFO DEV_NAME": Inserting module successful.\n");
	return 0;

sw_pwm__init__fail:
	hw_pwm__exit();
hw_pwm__init__fail:
	gpio__exit();
	
gpio_init__fail:
	unregister_chrdev(DEV_MAJOR, DEV_NAME);

	return result;
}


void servo_ctrl_exit(void) {
	printk(KERN_INFO DEV_NAME": Removing %s module\n", DEV_NAME);
	
	sw_pwm__exit();
	hw_pwm__exit();

	gpio__exit();

	unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

module_init(servo_ctrl_init);
module_exit(servo_ctrl_exit);

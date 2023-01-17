
#include <linux/module.h> // module_init(), module_exit()
#include <linux/fs.h> // file_operations
#include <linux/errno.h> // EFAULT
#include <linux/uaccess.h> // copy_from_user(), copy_to_user()

MODULE_LICENSE("Dual BSD/GPL");

#include "include/motor_ctrl.h"
#include "gpio.h"
#include "hw_pwm.h"
#include "sw_pwm.h"
#include "bldc.h"


#if MOTOR_CLTR__N_SERVO != (HW_PWM__N_CH+SW_PWM__N_CH)
#error "MOTOR_CLTR__N_SERVO wrong!"
#endif
#if MOTOR_CLTR__N_BLDC != BLDC__N_CH
#error "MOTOR_CLTR__N_BLDC wrong!"
#endif



static int motor_ctrl_open(struct inode *inode, struct file *filp) {
	return 0;
}

static int motor_ctrl_release(struct inode *inode, struct file *filp) {
	return 0;
}

static int16_t duty[MOTOR_CLTR__N_SERVO];

static ssize_t motor_ctrl_write(
	struct file* filp,
	const char *buf,
	size_t len,
	loff_t *f_pos

) {
	
	//TODO copy_from_user() -> duty
	
	uint8_t i, ch, thr;
	if(copy_from_user(duty, buf, len)==0)
	{
			for(i=0; i<MOTOR_CLTR__N_SERVO; i++){
				if(duty[i]>=0){
					bldc__set_dir(i, CW);
					thr=duty[i];
				}
				else{
					bldc__set_dir(i, CCW);
					thr=-duty[i];
				}	
					if(i<HW_PWM__N_CH){
				hw_pwm__set_threshold(i, thr << 1);
					}
					else{
				sw_pwm__set_threshold(i-HW_PWM__N_CH, thr << 1);
					}
			}
			return len;
	}
		else{
			return -EFAULT;
			}
	//TODO bldc__set_dir();
	void bldcset_dir(bldcch_t ch, dir_t dir) {
        bldc[ch].dir=dir;

        if(dir==CW)
        {
            //set
            gpioset(bldc[ch].dir_pin);
        }
        else
        {
            gpioclear(bldc[ch].dir_pin);
        //    clear
            }



}
	//TODO hw_pwm__set_threshold(ch, abs_duty << 1);
	for(i=0; i<2; i++){
            duty[i]=abs(duty[i]);
            duty[i]=duty[i]<<1;
        }
        hw_pwmset_threshold(HW_PWMCH_0, duty[0]);
        hw_pwmset_threshold(HW_PWMCH_1, duty[1]); 
        return len;
    }
    else
            return -EFAULT
}


static ssize_t motor_ctrl_read(
	struct file* filp,
	char* buf,
	size_t len,
	loff_t* f_pos
) {
	uint8_t i;
	motor_ctrl__read_arg_fb_t a;
	
	for(i=0; i<MOTOR_CLTR__N_SERVO; i++)
	{
		a.pos_fb[i] = duty[i];	
	}
	
	for(i=0; i<MOTOR_CLTR__N_BLDC; i++)
	{
		bldc__get_pulse_cnt(i, &a.pulse_cnt_fb[i]);	
	}
	
	if(copy_to_user(buf, &a, len)==0)
	{
		return len;
	}
	else
	{
		return -EFAULT;
	}
		
	
	//TODO duty -> a.pos_fb
	//TODO bldc__get_pulse_cnt() -> a.pulse_cnt_fb
	//TODO copy_to_user()
}


static long motor_ctrl_ioctl(
	struct file* filp,
	unsigned int cmd,
	unsigned long arg
) {
	motor_ctrl__ioctl_arg_moduo_t a;
	a = *(motor_ctrl__ioctl_arg_moduo_t*)&arg;
	
	//TODO Check cmd
	//TODO hw_pwm__set_moduo()

	return 0;
}

static struct file_operations motor_ctrl_fops = {
	open           : motor_ctrl_open,
	release        : motor_ctrl_release,
	read           : motor_ctrl_read,
	write          : motor_ctrl_write,
	unlocked_ioctl : motor_ctrl_ioctl
};


int motor_ctrl_init(void) {
	int result;
	uint8_t ch;

	printk(KERN_INFO DEV_NAME": Inserting module...\n");

	result = register_chrdev(DEV_MAJOR, DEV_NAME, &motor_ctrl_fops);
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
	
	
	// 10us*2000 -> 20ms.
	for(ch = 0; ch < HW_PWM__N_CH; ch++){
		hw_pwm__set_moduo(ch, 1000 << 1);
		hw_pwm__set_threshold(ch, 75 << 1);
	}
	
	
	result = bldc__init();
	if(result){
		printk(KERN_ERR DEV_NAME": bldc__init() failed!\n");
		goto bldc__init__fail;
	}
	
	
	printk(KERN_INFO DEV_NAME": Inserting module successful.\n");
	return 0;
	
bldc__init__fail:
	hw_pwm__exit();
hw_pwm__init__fail:
	gpio__exit();
	
gpio_init__fail:
	unregister_chrdev(DEV_MAJOR, DEV_NAME);

	return result;
}


void motor_ctrl_exit(void) {
	printk(KERN_INFO DEV_NAME": Removing %s module\n", DEV_NAME);
	
	servo_fb__exit();
	bldc__exit();
	sw_pwm__exit();
	hw_pwm__exit();

	gpio__exit();

	unregister_chrdev(DEV_MAJOR, DEV_NAME);
}

module_init(motor_ctrl_init);
module_exit(motor_ctrl_exit);

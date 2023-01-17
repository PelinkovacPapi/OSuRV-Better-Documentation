
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
#include "servo_fb.h"


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

static int16_t pos_cmd[MOTOR_CLTR__N_SERVO];

static ssize_t motor_ctrl_write(
	struct file* filp,
	const char *buf,
	size_t len,
	loff_t *f_pos
) {
	uint8_t ch;
	int16_t pc;
	uint32_t dp;
	
	if(copy_from_user((uint8_t*)pos_cmd + *f_pos, buf, len) != 0) {
		return -EFAULT;
	}else{
		for(ch = 0; ch < MOTOR_CLTR__N_SERVO; ch++){
			pc = pos_cmd[ch];
			
			// Extract direction.
			if(pc >= 0){
				bldc__set_dir(ch, CW);
				dp = pc;
			}else{
				bldc__set_dir(ch, CCW);
				dp = -pc;
			}
			
			// Protection.
			if(dp > 1000){
				dp = 1000;
			}
			
			// Shift for 1 because 2000 is period.
			if(ch < HW_PWM__N_CH){
				hw_pwm__set_threshold(ch, dp << 1);
			}else{
				sw_pwm__set_threshold(ch-HW_PWM__N_CH, dp << 1);
			}
		}
		
		*f_pos += len;
		return len;
	}
}


static ssize_t motor_ctrl_read(
	struct file* filp,
	char* buf,
	size_t len,
	loff_t* f_pos
) {
	motor_ctrl__read_arg_fb_t a;
	uint8_t ch;
	
	for(ch = 0; ch < MOTOR_CLTR__N_SERVO; ch++){
		//a.pos_fb[ch] = pos_cmd[ch]; // Loop pos cmd back.
		//TODO test
		// servo_fb__get_pos_fb(ch, &a.pos_fb[ch]);
	}
	for(ch = 0; ch < MOTOR_CLTR__N_BLDC; ch++){
		bldc__get_pulse_cnt(ch, &a.pulse_cnt_fb[ch]);
	}
	
	if(copy_to_user(buf, (uint8_t*)&a + *f_pos, len) != 0){
		return -EFAULT;
	}else{
		*f_pos += len;
		return len;
	}
}


static long motor_ctrl_ioctl(
    struct file* filp,
    unsigned int cmd,
    unsigned long arg
) {
    motor_ctrlioctl_arg_moduo_t a;

    switch(cmd){
        case IOCTL_MOTOR_CLTR_SET_MODUO:
            a = *(motor_ctrlioctl_arg_moduo_t*)&arg;
            if(a.ch < HW_PWMN_CH){
                hw_pwmset_moduo(a.ch, a.moduo);
            }else{
                sw_pwmset_moduo(a.ch-HW_PWMN_CH, a.moduo);
            }
            break;
        default:
            break;
    }

    return 0;
}

loff_t motor_ctrl_llseek(
	struct file* filp,
	loff_t offset,
	int whence
) {
	switch(whence){
		case SEEK_SET:
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			filp->f_pos += offset;
			break;
		case SEEK_END:
			return -ENOSYS; // Function not implemented.
		default:
			return -EINVAL;
		}
	return filp->f_pos;
}

static struct file_operations motor_ctrl_fops = {
	open           : motor_ctrl_open,
	release        : motor_ctrl_release,
	read           : motor_ctrl_read,
	write          : motor_ctrl_write,
	unlocked_ioctl : motor_ctrl_ioctl,
	llseek         : motor_ctrl_llseek
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
	
	result = sw_pwm__init();
	if(result){
		printk(KERN_ERR DEV_NAME": sw_pwm__init() failed!\n");
		goto sw_pwm__init__fail;
	}
	
	
	// 10us*2000 -> 20ms.
	for(ch = 0; ch < HW_PWM__N_CH; ch++){
		hw_pwm__set_moduo(ch, 1000 << 1);
		hw_pwm__set_threshold(ch, 75 << 1);
	}
	for(ch = 0; ch < SW_PWM__N_CH; ch++){
		sw_pwm__set_moduo(ch, 1000 << 1);
		sw_pwm__set_threshold(ch, 75 << 1);
	}
	
	
	result = bldc__init();
	if(result){
		printk(KERN_ERR DEV_NAME": bldc__init() failed!\n");
		goto bldc__init__fail;
	}
	
	result = servo_fb__init();
	if(result){
		printk(KERN_ERR DEV_NAME": servo_fb__init() failed!\n");
		goto servo_fb__init__fail;
	}
	
	
	printk(KERN_INFO DEV_NAME": Inserting module successful.\n");
	return 0;
	
servo_fb__init__fail:
	bldc__exit();
bldc__init__fail:
	sw_pwm__exit();
sw_pwm__init__fail:
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

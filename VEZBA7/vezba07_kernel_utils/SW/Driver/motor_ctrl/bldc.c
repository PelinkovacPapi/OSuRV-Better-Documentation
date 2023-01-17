
#include "bldc.h"
#include "gpio.h"


#include <linux/printk.h>
#include <linux/atomic.h>

//TODO Headers.
#include <linux/gpio.h> // gpio stuff
#include <linux/interrupt.h> // irq stuff

typedef struct {
	uint8_t dir_pin;
	uint8_t pg_pin;
	const char* pg_label;
	int pg_irq;
	dir_t dir;
	atomic64_t pulse_cnt;
} bldc_t;
static bldc_t bldc[] = {
	{
		17, // GPIO17, pin 11
		16, // GPIO16, pin 36
		"irq_gpio16",
		CW,
		0
	}
};


int bldcinit(void) {
    int result;
    bldcch_t ch;
    sw_pwm_t* ps;
    for(ch = 0; ch < BLDCN_CH; ch++){
        //TODO Init direction pin
        gpioclear(17);
        gpio_steer_pinmux(17, GPIO_OUT);
        //TODO Init timer
        ps = &sw_pwms[ch];

        ps->pin = pins[ch];
        gpioclear(ps->pin);
        gpiosteer_pinmux(ps->pin, GPIOOUT);

        ps->on = true;

        spin_lock_init(&ps->interval_pending_lock);

        ps->moduo = 1000;
        ps->threshold = 0;
        set_intervals(ps);
        ps->on_interval = ps->on_interval_pending;
        ps->off_interval = ps->off_interval_pending;

        hrtimer_init(
            &ps->timer,
            CLOCK_MONOTONIC,
            HRTIMER_MODE_REL_HARD
        );
        sw_pwms[ch].timer.function = &timer_callback;
        hrtimer_start(
            &ps->timer,
            ps->off_interval,
            HRTIMER_MODE_REL_HARD
        );

        //TODO Init IRQ
            result = gpio_request_one(
            bldc[ch].pg_pin,
            GPIOF_IN,
            bldc[ch].pg_label
        );
        if(r){
            printk(
                KERN_ERR DEV_NAME": %s(): gpio_request_one() failed!\n",
                func
            );
            goto exit;
        }

        bldc[ch].pg_irq = gpio_to_irq(bldc[ch].pg_pin);
        result = request_irq(
            bldc[ch].pg_irq,
            pg_isr,
            IRQF_TRIGGER_FALLING,
            bldc[ch].pg_label,
            &bldc[ch]
        );
        if(result){
            printk(
                KERN_ERR DEV_NAME": %s(): request_irq() failed!\n",
                func
            );
            goto exit;
        }
    }

exit:
    if(result){
        printk(KERN_ERR DEV_NAME": %s() failed with %d!\n", func, r);
        bldcexit();
    }

    return 0;
}

void bldcexit(void) {
    bldcch_t ch;

    for(ch = 0; ch < BLDC__N_CH; ch++){
        disable_irq(bldc[ch].pg_irq);
        free_irq(bldc[ch].pg_irq, &bldc[ch]);
        gpio_free(bldc[ch].pg_pin);
    }
}


void bldc__set_dir(bldc__ch_t ch, dir_t dir) {
	// TODO Save and set direction @ GPIO17, pin 11
	if(ch < BLDC__N_CH){
		bldc[ch].dir = dir;
		if(dir == CW){
			gpio__set(bldc[ch].dir_pin);
		}else{
			gpio__clear(bldc[ch].dir_pin);
		}
	}
	
}

void bldcset_duty(bldcch_t ch, u16 duty_permille) {
    if(ch >= BLDC__N_CH){
        return;
    }
}

void bldcget_pulse_cnt(bldcch_t ch, int64_t* pulse_cnt) {
    if(ch >= BLDC__N_CH){
        return;
    }

    int64_t pulse_cnt2 = bldc[ch].pulse_cnt;

    if(pulse_cnt2 != bldc[ch].pulse_cnt){
        pulse_cnt2 = bldc[ch].pulse_cnt;
    }
    *pulse_cnt = pulse_cnt2;
}

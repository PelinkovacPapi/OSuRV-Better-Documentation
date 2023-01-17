
#include "bldc.h"

#include "gpio.h"

#include <linux/gpio.h> // gpio sutff
#include <linux/interrupt.h> // irq stuff
#include <linux/printk.h>


typedef struct {
	uint8_t dir_pin;
	uint8_t pg_pin;
	const char* pg_label;
	int pg_irq;
	dir_t dir;
	volatile int64_t pulse_cnt;
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


int bldc__init(void) {
	int result;
	bldc__ch_t ch;
	
	for(ch = 0; ch < BLDC__N_CH; ch++){
		gpio__steer_pinmux(bldc[ch].dir_pin, GPIO__OUT);
		bldc__set_dir(ch, CW);
	}
	
	
	return 0;
}

void bldc__exit(void) {
	bldc__ch_t ch;
	
	for(ch = 0; ch < BLDC__N_CH; ch++){
		gpio_free(bldc[ch].pg_pin);
	}
}


void bldc__set_dir(bldc__ch_t ch, dir_t dir) {
	if(ch < BLDC__N_CH){
		bldc[ch].dir = dir;
		if(dir == CW){
			gpio__set(bldc[ch].dir_pin);
		}else{
			gpio__clear(bldc[ch].dir_pin);
		}
	}
}

void bldc__get_pulse_cnt(bldc__ch_t ch, int64_t* pulse_cnt) {

}

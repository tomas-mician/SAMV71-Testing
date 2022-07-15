#include <atmel_start.h>
#include <stdio.h>
#include "tc_lite.h"
#include "hal_gpio.h"

bool read_trig_channel(const uint8_t ch)
{
	bool status = gpio_get_pin_level(ch);
	// printf("trigger status: %s", status ? "true" : "false"); // need to set up serial for this to work
	return status;
}

int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();
	delay_init(TC2);
	// TC channel 0: input - external trigger 
	// TC channel 1: we might not need this
	// TC channel 2: output - generating PWM waveform 
	
	/* Replace with your application code */
	while (1) {
		// start channel 2 if detection
		bool ch_status = read_trig_channel(PB3_TRIG);
		if (ch_status) 
		{
			start_timer(TC2, 2);
		}
		else {
			stop_timer(TC2, 2);
		}
		//delay_ms(500); // Testing only 
	}
}
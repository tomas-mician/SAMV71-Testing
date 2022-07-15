/**
 * \file
 *
 * \brief Application implement
 *
 * Copyright (c) 2019 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */

#include <atmel_start.h>
#include <stdio.h>
#include "tc_lite_driver_example_config.h"
#include "tc_lite.h"
#include "hal_gpio.h"

bool ch0_status = false;
bool ch1_status = false;
bool ch2_status = false;
bool ch3_status = false;

/* TC channel 0 interrupt callback function */
void channel_0_cb(uint32_t status)
{
	/* Toggle on board LED0 */
	//gpio_toggle_pin_level(LED0);
	gpio_toggle_pin_level(TCTESTPIN);
}

void update_channels_status(void)
{
	 ch0_status = gpio_get_pin_level(GPIO_IN_CH0);
	 //printf("%s", ch0_status ? "true" : "false");
	 
	 ch1_status = gpio_get_pin_level(GPIO_IN_CH1);
	 ch2_status = gpio_get_pin_level(GPIO_IN_CH2);
	 ch3_status = gpio_get_pin_level(GPIO_IN_CH3);
}

void check_status(void)
{
	if (gpio_get_pin_level(GPIO_IN_CH0)) {
		 //start_timer(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 1);
		 gpio_toggle_pin_level(TCTESTPIN);
		 //delay_ms(500);
	}
}

int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	/* Register callback function for TC Channel 0 interrupt */
	tc_register_callback(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 0, channel_0_cb); //why do we need this? // this is for driving pin with TC in capture mode... 

	/* Start TC channel 2 - configured in Waveform mode, generate PWM waveform and used as clock source to TC channel 0
	 * and 1 */
	start_timer(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 2); //why do we need this? 

	/* Start TC channel 0 - configured in Capture mode and generate periodic interrupt */ // Not needed for TC/trigger test
	// start_timer(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 0);

	/* Start TC channel 1 - configured in Waveform mode and generate PWM waveform on GPIO pin */
	//start_timer(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 1);
	
	//check_status();
	while(1) {
		check_status();
	}

	while (0) {
		update_channels_status(); // Check if any channel has detected something constantly 
		// check status of each channel
		// if status = true, start corresponding TC channel
		// note: start = output just 1 pulse. don't need periodic or continuous output
		if (ch0_status) {
			start_timer(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 1);
			//delay_ms(1000);
			// set a delay? does it need one? 	
		}
		if (ch1_status) {
			// todo: set up ch1 TC output
		}
		if (ch2_status) {
			// todo: set up ch2 TC output
		}
		if (ch3_status) {
			// todo: need to set up a 4th channel
		}
		else {
			stop_timer(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 1); // maybe not the way to do it... 
			//gpio_toggle_pin_level(LED0);
			//gpio_toggle_pin_level(TCTESTPIN);
		}
	}
}

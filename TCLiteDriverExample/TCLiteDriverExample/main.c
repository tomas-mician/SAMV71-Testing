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
#include "tc_lite_driver_example_config.h"
#include "tc_lite.h"
#include "hal_gpio.h"

/* TC channel 0 interrupt callback function */
void channel_0_cb(uint32_t status)
{
	/* Toggle on board LED0 */
	gpio_toggle_pin_level(LED0);
}



int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	/* Register callback function for TC Channel 0 interrupt */
	//tc_register_callback(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 0, channel_0_cb);

	/* Start TC channel 2 - configured in Waveform mode, generate PWM waveform and used as clock source to TC channel 0
	 * and 1 */
	start_timer(TC2, 2);
	start_timer(TC1, 0);

	/* Start TC channel 0 - configured in Capture mode and generate periodic interrupt */
	//start_timer(TC_LITE_DRIVER_EXAMPLE_INSTANCE, 0);

	/* Start TC channel 1 - configured in Waveform mode and generete PWM waveform on GPIO pin */
	start_timer(TC2, 1);
	start_timer(TC1, 2);
	// MUX_PC30B_TC1_TIOB5 --> TC1, try channel 2
	//start_timer(TC1, 2);
	while (1) {
	}
}

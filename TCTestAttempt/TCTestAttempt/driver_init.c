/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */

#include "driver_init.h"
#include <hal_init.h>
#include <hpl_pmc.h>
#include <peripheral_clk_config.h>
#include <utils.h>

void EXTERNAL_IRQ_0_init(void)
{
}

void TIMER_0_PORT_init(void)
{
	//gpio_set_pin_function(PA6_OUT, MUX_PC9B_TC2_TIOB7); // pin needs to match - 
	gpio_set_pin_function(PC30_OUT, MUX_PC30B_TC1_TIOB5); // TIOB5 is on TC1 peripheral. Need to change stuff to match this
}

void TIMER_0_CLOCK_init(void)
{
	_pmc_enable_periph_clock(ID_TC2_CHANNEL0);
	_pmc_enable_periph_clock(ID_TC2_CHANNEL1);
	_pmc_enable_periph_clock(ID_TC2_CHANNEL2);
}

void delay_driver_init(void)
{
	delay_init(SysTick);
}

/* The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
 * for low speed and full speed operation. */
#if (CONF_USBHS_SRC == CONF_SRC_USB_48M)
#if (CONF_USBHS_FREQUENCY > (48000000 + 48000000 / 400)) || (CONF_USBHS_FREQUENCY < (48000000 - 48000000 / 400))
#warning USB clock should be 48MHz ~ 0.25% clock, check your configuration!
#endif
#endif

void USB_0_CLOCK_init(void)
{
	_pmc_enable_periph_clock(ID_USBHS);
}

void USB_0_init(void)
{
	USB_0_CLOCK_init();
	usb_d_init();
}

void system_init(void)
{
	init_mcu();

	/* Disable Watchdog */
	hri_wdt_set_MR_WDDIS_bit(WDT);

	EXTERNAL_IRQ_0_init();
	
	
	// TC channel 0: input - external trigger
	// TC channel 1: we might not need this
	// TC channel 2: output - generating PWM waveform - 
	// is it right to set direction here vs starting clock specifically? might be problem? 
	gpio_set_pin_direction(PB3_TRIG, GPIO_DIRECTION_IN);
	//gpio_set_pin_direction(PA6_OUT, GPIO_DIRECTION_OUT);
	

	TIMER_0_CLOCK_init();
	TIMER_0_PORT_init();
	TIMER_0_init();

	delay_driver_init();

	USB_0_init();

	ext_irq_init();
}

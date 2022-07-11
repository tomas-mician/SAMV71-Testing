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

void TIMER_0_PORT_init(void)
{

	gpio_set_pin_function(LED1, MUX_PC9B_TC2_TIOB7);
}

void TIMER_0_CLOCK_init(void)
{
	_pmc_enable_periph_clock(ID_TC2_CHANNEL0);
	_pmc_enable_periph_clock(ID_TC2_CHANNEL1);
	_pmc_enable_periph_clock(ID_TC2_CHANNEL2);
}

void system_init(void)
{
	init_mcu();

	_pmc_enable_periph_clock(ID_PIOA);

	/* Disable Watchdog */
	hri_wdt_set_MR_WDDIS_bit(WDT);

	/* GPIO on PA23 */

	gpio_set_pin_level(LED0,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   true);

	// Set pin direction to output
	gpio_set_pin_direction(LED0, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(LED0, GPIO_PIN_FUNCTION_OFF);

	TIMER_0_CLOCK_init();
	TIMER_0_PORT_init();
	TIMER_0_init();
}

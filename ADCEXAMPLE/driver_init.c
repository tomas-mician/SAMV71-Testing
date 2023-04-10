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
#include <hpl_usart_base.h>

struct adc_sync_descriptor BATTERY_ADC;

struct usart_sync_descriptor TARGET_IO;

void BATTERY_ADC_PORT_init(void)
{
}

void BATTERY_ADC_CLOCK_init(void)
{

	_pmc_enable_periph_clock(ID_AFEC0);
}

void BATTERY_ADC_init(void)
{
	BATTERY_ADC_CLOCK_init();
	BATTERY_ADC_PORT_init();
	adc_sync_init(&BATTERY_ADC, AFEC0, (void *)NULL);
}

void TARGET_IO_PORT_init(void)
{

	gpio_set_pin_function(PA21, MUX_PA21A_USART1_RXD1);

	gpio_set_pin_function(PB4, MUX_PB4D_USART1_TXD1);
}

void TARGET_IO_CLOCK_init(void)
{
	_pmc_enable_periph_clock(ID_USART1);
}

void TARGET_IO_init(void)
{
	TARGET_IO_CLOCK_init();
	TARGET_IO_PORT_init();
	usart_sync_init(&TARGET_IO, USART1, _usart_get_usart_sync());
}

void system_init(void)
{
	init_mcu();

	/* Disable Watchdog */
	hri_wdt_set_MR_WDDIS_bit(WDT);

	BATTERY_ADC_init();

	TARGET_IO_init();
}

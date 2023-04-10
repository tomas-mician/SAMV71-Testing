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

/*! The buffer size for USART */
#define USART_0_BUFFER_SIZE 16

struct usart_async_descriptor USART_0;

static uint8_t USART_0_buffer[USART_0_BUFFER_SIZE];

void delay_driver_init(void)
{
	delay_init(SysTick);
}

/**
 * \brief USART Clock initialization function
 *
 * Enables register interface and peripheral clock
 */
void USART_0_CLOCK_init()
{
	_pmc_enable_periph_clock(ID_USART1);
}

/**
 * \brief USART pinmux initialization function
 *
 * Set each required pin to USART functionality
 */
void USART_0_PORT_init()
{

	gpio_set_pin_function(PA21, MUX_PA21A_USART1_RXD1);

	gpio_set_pin_function(PB4, MUX_PB4D_USART1_TXD1);
}

/**
 * \brief USART initialization function
 *
 * Enables USART peripheral, clocks and initializes USART driver
 */
void USART_0_init(void)
{
	USART_0_CLOCK_init();
	USART_0_PORT_init();
	usart_async_init(&USART_0, USART1, USART_0_buffer, USART_0_BUFFER_SIZE, _usart_get_usart_async());
}

void system_init(void)
{
	init_mcu();

	/* Disable Watchdog */
	hri_wdt_set_MR_WDDIS_bit(WDT);

	delay_driver_init();
	USART_0_init();
}

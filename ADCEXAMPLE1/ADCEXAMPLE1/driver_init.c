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

/* The priority of the peripheral should be between the low and high interrupt priority set by chosen RTOS,
 * Otherwise, some of the RTOS APIs may fail to work inside interrupts
 * In case of FreeRTOS,the Lowest Interrupt priority is set by configLIBRARY_LOWEST_INTERRUPT_PRIORITY and
 * Maximum interrupt priority by configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, So Interrupt Priority of the peripheral
 * should be between configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY and configLIBRARY_LOWEST_INTERRUPT_PRIORITY
 */
#define PERIPHERAL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY - 1)

struct adc_sync_descriptor BATTERY_ADC;

struct usart_os_descriptor TARGET_IO;
uint8_t                    TARGET_IO_buffer[TARGET_IO_BUFFER_SIZE];

void BATTERY_ADC_PORT_init(void)
{

	gpio_set_pin_function(PA19, GPIO_PIN_FUNCTION_OFF);
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

	gpio_set_pin_function(PA24, MUX_PA24A_USART1_RTS1);

	gpio_set_pin_function(PB4, MUX_PB4D_USART1_TXD1);
}

void TARGET_IO_CLOCK_init(void)
{
	_pmc_enable_periph_clock(ID_USART1);
}

void TARGET_IO_init(void)
{
	TARGET_IO_CLOCK_init();
	usart_os_init(&TARGET_IO, USART1, TARGET_IO_buffer, TARGET_IO_BUFFER_SIZE, (void *)_usart_get_usart_async());
	NVIC_SetPriority(USART1_IRQn, PERIPHERAL_INTERRUPT_PRIORITY);
	usart_os_enable(&TARGET_IO);
	TARGET_IO_PORT_init();
}

void system_init(void)
{
	init_mcu();

	/* Disable Watchdog */
	hri_wdt_set_MR_WDDIS_bit(WDT);

	BATTERY_ADC_init();

	TARGET_IO_init();
}

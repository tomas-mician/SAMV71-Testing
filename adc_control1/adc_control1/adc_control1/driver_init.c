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
#include <hpl_spi_base.h>
#include <hpl_usart_base.h>

/*! The buffer size for USART */
#define EDBG_COM_BUFFER_SIZE 16

struct spi_m_sync_descriptor  SPI_0;
struct usart_async_descriptor EDBG_COM;

static uint8_t EDBG_COM_buffer[EDBG_COM_BUFFER_SIZE];

volatile unsigned convert_complete_flag = 0 ;


void EXTERNAL_IRQ_0_init(void)
{

	// Set pin direction to input
	gpio_set_pin_direction(PB2, GPIO_DIRECTION_IN);

	gpio_set_pin_pull_mode(PB2,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(PB2, GPIO_PIN_FUNCTION_OFF);
}

void SPI_0_PORT_init(void)
{

	gpio_set_pin_function(PD20, MUX_PD20B_SPI0_MISO);

	gpio_set_pin_function(PD21, MUX_PD21B_SPI0_MOSI);

	gpio_set_pin_function(PD22, MUX_PD22B_SPI0_SPCK);
}

void SPI_0_CLOCK_init(void)
{
	_pmc_enable_periph_clock(ID_SPI0);
}

void SPI_0_init(void)
{
	SPI_0_CLOCK_init();
	spi_m_sync_set_func_ptr(&SPI_0, _spi_get_spi_m_sync());
	spi_m_sync_init(&SPI_0, SPI0);
	SPI_0_PORT_init();
}

/**
 * \brief USART Clock initialization function
 *
 * Enables register interface and peripheral clock
 */
void EDBG_COM_CLOCK_init()
{
	_pmc_enable_periph_clock(ID_USART1);
}

/**
 * \brief USART pinmux initialization function
 *
 * Set each required pin to USART functionality
 */
void EDBG_COM_PORT_init()
{

	gpio_set_pin_function(EDBG_COM_RX, MUX_PA21A_USART1_RXD1);

	gpio_set_pin_function(EDBG_COM_TX, MUX_PB4D_USART1_TXD1);
}

/**
 * \brief USART initialization function
 *
 * Enables USART peripheral, clocks and initializes USART driver
 */
void EDBG_COM_init(void)
{
	EDBG_COM_CLOCK_init();
	EDBG_COM_PORT_init();
	usart_async_init(&EDBG_COM, USART1, EDBG_COM_buffer, EDBG_COM_BUFFER_SIZE, _usart_get_usart_async());
}

static void convert_complete_intr(void)
{
	convert_complete_flag = 1;
}


void system_init(void)
{
	init_mcu();

	_pmc_enable_periph_clock(ID_PIOA);

	_pmc_enable_periph_clock(ID_PIOB);

	/* Disable Watchdog */
	hri_wdt_set_MR_WDDIS_bit(WDT);

	/* GPIO */

	gpio_set_pin_level(LED0,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   true);

	gpio_set_pin_direction(LED0, GPIO_DIRECTION_OUT);
	gpio_set_pin_function(LED0, GPIO_PIN_FUNCTION_OFF);

	gpio_set_pin_level(ADC_CSn_3V, true);
	gpio_set_pin_direction(ADC_CSn_3V, GPIO_DIRECTION_OUT);
	gpio_set_pin_function(ADC_CSn_3V, GPIO_PIN_FUNCTION_OFF);

	gpio_set_pin_level(DAC_CSn_3V, true);
	gpio_set_pin_direction(DAC_CSn_3V, GPIO_DIRECTION_OUT);
	gpio_set_pin_function(DAC_CSn_3V, GPIO_PIN_FUNCTION_OFF);

	gpio_set_pin_level(DAC_LDACn, true);
	gpio_set_pin_direction(DAC_LDACn, GPIO_DIRECTION_OUT);
	gpio_set_pin_function(DAC_LDACn, GPIO_PIN_FUNCTION_OFF);

	gpio_set_pin_level(FF_Enable_5V, false);
	gpio_set_pin_direction(FF_Enable_5V, GPIO_DIRECTION_OUT);
	gpio_set_pin_function(FF_Enable_5V, GPIO_PIN_FUNCTION_OFF);
	
	gpio_set_pin_direction(ADC_BUSYn_5V, GPIO_DIRECTION_IN);
	gpio_set_pin_pull_mode(ADC_BUSYn_5V, GPIO_PULL_OFF);
	gpio_set_pin_function(ADC_BUSYn_5V, GPIO_PIN_FUNCTION_OFF);

	EXTERNAL_IRQ_0_init();

	SPI_0_init();
	EDBG_COM_init();

	ext_irq_init();

	ext_irq_register(ADC_BUSYn_5V, convert_complete_intr);
//	ext_irq_enable(ADC_BUSYn_5V);

}

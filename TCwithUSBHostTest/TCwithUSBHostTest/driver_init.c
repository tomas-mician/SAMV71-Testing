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

struct usart_sync_descriptor EDBG_COM;

struct usb_h_desc USB_HOST_INSTANCE_inst;

void EDBG_COM_PORT_init(void)
{

	gpio_set_pin_function(EDBG_COM_RX, MUX_PA21A_USART1_RXD1);

	gpio_set_pin_function(EDBG_COM_TX, MUX_PB4D_USART1_TXD1);
}

void EDBG_COM_CLOCK_init(void)
{
	_pmc_enable_periph_clock(ID_USART1);
}

void EDBG_COM_init(void)
{
	EDBG_COM_CLOCK_init();
	EDBG_COM_PORT_init();
	usart_sync_init(&EDBG_COM, USART1, _usart_get_usart_sync());
}

void TIMER_PORT_init(void)
{
	// Need a pair of TIOAx, TIOBx 
	// TIOAx: output
	gpio_set_pin_function(TC_OUT, MUX_PD21C_TC3_TIOA11);
	// TIOBx: event trigger/input
	gpio_set_pin_function(TC_TRIG, MUX_PD22C_TC3_TIOB11);
}

void TIMER_CLOCK_INIT(void)
{
	_pmc_enable_periph_clock(ID_TC3_CHANNEL0);
	_pmc_enable_periph_clock(ID_TC3_CHANNEL1);
	_pmc_enable_periph_clock(ID_TC3_CHANNEL2);
}

/* Pipe instance pool */
static struct usb_h_pipe USB_HOST_INSTANCE_pipe[CONF_USB_H_NUM_PIPE_SP];
/* Private instance for USBHS host driver */
static struct _usb_h_prvt USB_HOST_INSTANCE_prvt;

/* The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
 * for low speed and full speed operation. */
#if (CONF_USBHS_SRC == CONF_SRC_USB_48M)
#if (CONF_USBHS_FREQUENCY > (48000000 + 48000000 / 400)) || (CONF_USBHS_FREQUENCY < (48000000 - 48000000 / 400))
#warning USB clock should be 48MHz ~ 0.25% clock, check your configuration!
#endif
#endif

/** \brief VBus control callback
 *  VBus must be always turned on if this callback is disabled.
 */
WEAK void my_vbus_ctrl_func(void *drv, uint8_t port, bool onoff)
{
	/* Turn on/off your root hub port power */
}

void USB_HOST_INSTANCE_CLOCK_init(void)
{
	hri_pmc_write_SCER_reg(PMC, PMC_SCER_USBCLK);
	_pmc_enable_periph_clock(ID_USBHS);
}

void USB_HOST_INSTANCE_init(void)
{
	USB_HOST_INSTANCE_CLOCK_init();
	_usbhcd_prvt_init(
	    &USB_HOST_INSTANCE_prvt, USB_HOST_INSTANCE_pipe, sizeof(USB_HOST_INSTANCE_pipe) / sizeof(struct usb_h_pipe));
	usb_h_init(&USB_HOST_INSTANCE_inst, USBHS, &USB_HOST_INSTANCE_prvt);
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

	EDBG_COM_init();

	USB_HOST_INSTANCE_init();
	
	// Timer counter peripheral setup
	TIMER_CLOCK_INIT();
	TIMER_PORT_init();
}

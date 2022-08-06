/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef ATMEL_START_PINS_H_INCLUDED
#define ATMEL_START_PINS_H_INCLUDED

#include <hal_gpio.h>

// SAMV71 has 4 pin functions

#define GPIO_PIN_FUNCTION_A 0
#define GPIO_PIN_FUNCTION_B 1
#define GPIO_PIN_FUNCTION_C 2
#define GPIO_PIN_FUNCTION_D 3

#define EDBG_COM_RX GPIO(GPIO_PORTA, 21)
#define LED0 GPIO(GPIO_PORTA, 23)
#define EDBG_COM_TX GPIO(GPIO_PORTB, 4)

// Pins for TC peripheral
#define TC_TRIG GPIO(GPIO_PORTD, 22) // TIOBx
#define TC_OUT GPIO(GPIO_PORTD, 21) // TIOAx

#endif // ATMEL_START_PINS_H_INCLUDED

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

// Pins for TC channels
//#define PC6_TRIG GPIO(GPIO_PORTC, 6) // TC ch0: external trigger
//#define PC9_OUT GPIO(GPIO_PORTC, 9) // TC ch2: output

#define PB3_TRIG GPIO(GPIO_PORTB, 3) // TC ch0: external trigger
#define PA6_OUT GPIO(GPIO_PORTA, 6) // TC ch2: output


#endif // ATMEL_START_PINS_H_INCLUDED

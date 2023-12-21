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
#define PB2 GPIO(GPIO_PORTB, 2)
#define EDBG_COM_TX GPIO(GPIO_PORTB, 4)
#define PD20 GPIO(GPIO_PORTD, 20)
#define PD21 GPIO(GPIO_PORTD, 21)
#define PD22 GPIO(GPIO_PORTD, 22)

#define ADC_CSn_3V    GPIO(GPIO_PORTB, 3)
#define DAC_CSn_3V    GPIO(GPIO_PORTD, 25)
#define DAC_LDACn     GPIO(GPIO_PORTC, 30)
#define FF_Enable_5V  GPIO(GPIO_PORTA, 0)
#define ADC_BUSYn_5V  GPIO(GPIO_PORTB, 2)



#endif // ATMEL_START_PINS_H_INCLUDED

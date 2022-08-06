/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file or main.c
 * to avoid loosing it when reconfiguring.
 */
#ifndef USB_DEVICE_MAIN_H
#define USB_DEVICE_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "usbhc.h"

extern struct usbhc_driver USB_HOST_CORE_INSTANCE_inst;

void USB_HOST_CORE_INSTANCE_example(void);

#include "cdchf_acm.h"

extern struct cdchf_acm USB_HOST_CDC_ACM_1_inst;

/**
 * \brief Example of using USB Host CDC ACM.
 */
void USB_HOST_CDC_ACM_1_example(void);

extern struct cdchf_acm USB_HOST_CDC_ACM_0_inst;

/**
 * \berif Initialize USB
 */
void usb_init(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // USB_DEVICE_MAIN_H

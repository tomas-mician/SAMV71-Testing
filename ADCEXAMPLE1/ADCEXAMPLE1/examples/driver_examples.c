/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */

#include "driver_examples.h"
#include "driver_init.h"
#include "utils.h"

/**
 * Example of using BATTERY_ADC to generate waveform.
 */
void BATTERY_ADC_example(void)
{

	while (1) {
	}
}

#define TASK_TARGET_IO_STACK_SIZE (300 / sizeof(portSTACK_TYPE))
#define TASK_TARGET_IO_STACK_PRIORITY (tskIDLE_PRIORITY + 1)

static TaskHandle_t xCreatedtarget_ioTask;
static uint8_t      example_TARGET_IO[12] = "Hello World!";

/**
 * Example task of using TARGET_IO to echo using the IO abstraction.
 */
static void TARGET_IO_example_task(void *param)
{
	struct io_descriptor *io;
	uint16_t              data;

	(void)param;

	usart_os_get_io(&TARGET_IO, &io);
	io_write(io, example_TARGET_IO, 12);

	for (;;) {
		if (io_read(io, (uint8_t *)&data, 2) == 2) {
			io_write(io, (uint8_t *)&data, 2);
		}
		os_sleep(10);
	}
}

/**
 * \brief Create OS task for TARGET_IO
 */
void task_target_io_create()
{
	/* Create task for TARGET_IO */
	if (xTaskCreate(TARGET_IO_example_task,
	                "target_io",
	                TASK_TARGET_IO_STACK_SIZE,
	                NULL,
	                TASK_TARGET_IO_STACK_PRIORITY,
	                &xCreatedtarget_ioTask)
	    != pdPASS) {
		while (1) {
			/* Please checkup stack and FreeRTOS configuration. */
		}
	}
	/* Call vTaskStartScheduler() function in main function. Place vTaskStartScheduler function call after creating all
	 * tasks and before while(1) in main function */
}

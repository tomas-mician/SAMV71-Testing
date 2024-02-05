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
 * Example of using SPI_0 to write "Hello World" using the IO abstraction.
 */
static uint8_t example_SPI_0[12] = "Hello World!";

void SPI_0_example(void)
{
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor(&SPI_0, &io);

	spi_m_sync_enable(&SPI_0);
	io_write(io, example_SPI_0, 12);
}

#define TASK_USART_0_STACK_SIZE (300 / sizeof(portSTACK_TYPE))
#define TASK_USART_0_STACK_PRIORITY (tskIDLE_PRIORITY + 1)

static TaskHandle_t xCreatedusart_0Task;
static uint8_t      example_USART_0[12] = "Hello World!";

/**
 * Example task of using USART_0 to echo using the IO abstraction.
 */
static void USART_0_example_task(void *param)
{
	struct io_descriptor *io;
	uint16_t              data;

	(void)param;

	usart_os_get_io(&USART_0, &io);
	io_write(io, example_USART_0, 12);

	for (;;) {
		if (io_read(io, (uint8_t *)&data, 2) == 2) {
			io_write(io, (uint8_t *)&data, 2);
		}
		os_sleep(10);
	}
}

/**
 * \brief Create OS task for USART_0
 */
void task_usart_0_create()
{
	/* Create task for USART_0 */
	if (xTaskCreate(USART_0_example_task,
	                "usart_0",
	                TASK_USART_0_STACK_SIZE,
	                NULL,
	                TASK_USART_0_STACK_PRIORITY,
	                &xCreatedusart_0Task)
	    != pdPASS) {
		while (1) {
			/* Please checkup stack and FreeRTOS configuration. */
		}
	}
	/* Call vTaskStartScheduler() function in main function. Place vTaskStartScheduler function call after creating all
	 * tasks and before while(1) in main function */
}

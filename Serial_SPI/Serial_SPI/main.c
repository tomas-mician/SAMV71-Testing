#include <atmel_start.h>

#include <avr/interrupt.h>
#include "bme280.h"
// Include your microcontroller's SPI and GPIO headers here




// TIMER SHENANIGANS
typedef void (*timer_task_callback_t)(const struct timer_task *const timer_task);
struct timer_task {
	timer_task_callback_t callback;
	uint32_t interval; // Interval for the task in milliseconds
	uint8_t mode;      // Operation mode, e.g., repeat
	// You can add more fields as needed
};
#include <stdint.h> // For fixed-width integers
#include <stdbool.h> // For the `bool` type
#include <string.h> // For memcpy
// Assuming these are defined or included from your timer library
extern void timer_init(void);
extern void timer_add_task(struct timer_task* task);
extern void timer_start(void);
static struct timer_task task, micro_task;
volatile bool send_data_flag = false, read_bme280_flag = false;
uint16_t messageCounter = 0, milliCounter = 0, microCounter = 0;
uint32_t secondCounter = 0;
const uint16_t message_interval_ms = 100;
// Timer task callbacks
static void timer_task_cb(const struct timer_task *const timer_task);
static void micro_timer_task_cb(const struct timer_task *const timer_task);
void setup_timers(void) {
	// Initialize the timer module
	timer_init();
	// Setup millisecond timer task
	task.callback = timer_task_cb;
	task.interval = 1; // 1 millisecond interval
	task.mode = TIMER_TASK_REPEAT; // Repeat the task
	// Add tasks to the timer
	timer_add_task(&task);
	timer_add_task(&micro_task);
	// Start the timer
	timer_start();
}
// Timer Callback Implementation
static void timer_task_cb(const struct timer_task *const timer_task) {
	milliCounter++;
	messageCounter++;
	// Check if it's time to send a message
	if (messageCounter >= message_interval_ms) {
		// Send serial message or set flag to indicate data should be sent
		send_data_flag = true;
		write_flag = true; // Set the flag to enable writing
		io_write("Timer tick.\n"); // Write a message to the buffer
		write_flag = false; // Reset the flag after writing
		messageCounter = 0; // Reset message counter
	}
	// Reset millisecond counter every second
	if (milliCounter >= 1000) {
		milliCounter = 0;
		secondCounter++;
		read_bme280_flag = true; // Set flag to read BME280 sensor data
	}
}
// serialization
#define BUFFER_SIZE 1024
static char serial_buffer[BUFFER_SIZE];
static int buffer_index = 0;
static bool write_flag = false;
void io_write(const char* data) {
	if (write_flag && (buffer_index < (BUFFER_SIZE - strlen(data)))) {
		memcpy(&serial_buffer[buffer_index], data, strlen(data));
		buffer_index += strlen(data);
		serial_buffer[buffer_index] = '\0'; // Ensure null-termination
	}
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


















int main(void) {
	
	struct bme280_dev dev;
	
	dev.intf_ptr = &Humidity_CS; // Use if you need to pass the CS pin or SPI handler
	
	// Initialize MCU, drivers, and middleware
	// This would typically initialize system clocks, peripherals, and any middleware needed
	atmel_start_init();
	// Setup and start timer tasks
	setup_timers();
	// Main loop
	
	
	//sei();
	
	

	
	struct bme280_dev dev;
	int8_t rslt = BME280_OK;

	// Initialize your SPI interface here (platform-specific code)

	// Device initialization with SPI interface
	dev.intf = BME280_SPI_INTF;
	dev.read = user_spi_read;
	dev.write = user_spi_write;
	dev.delay_us = user_delay_us;
	// Optionally, pass a pointer to your SPI interface or CS pin configuration through dev.intf_ptr

	// Initialize the BME280
	rslt = bme280_init(&dev);
	if (rslt != BME280_OK) {
		// Handle error
	}

	// Sensor configuration
	struct bme280_settings settings;
	settings.osr_h = BME280_OVERSAMPLING_1X;
	settings.osr_p = BME280_OVERSAMPLING_16X;
	settings.osr_t = BME280_OVERSAMPLING_2X;
	settings.filter = BME280_FILTER_COEFF_4;
	settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

	uint8_t settings_sel = BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM | BME280_SEL_FILTER | BME280_SEL_STANDBY;
	rslt = bme280_set_sensor_settings(settings_sel, &settings, &dev);
	if (rslt != BME280_OK) {
		// Handle error
	}

	// Set the sensor to normal mode
	rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
	if (rslt != BME280_OK) {
		// Handle error
	}

	// Read sensor data
	struct bme280_data comp_data;
	while (1) {
		
		// Check if the buffer contains serialized data
		if (serial_buffer[0] != '\0') {
			// Print the buffer's content to verify the timer task is working
			printf("Buffer content: %s", serial_buffer);
			// Clear the buffer after printing its contents
			memset(serial_buffer, 0, BUFFER_SIZE);
			buffer_index = 0; // Reset the buffer index for new data
		}
		// Implement a small delay to prevent overwhelming the output
		// This delay mechanism will depend on your platform
		// For a simple simulation, you might use a busy wait or a platform-specific sleep function
		// Example: platform_specific_delay_function(100); // Delay for 100ms or similar
		
		
		
		
		
		// Delay to meet the standby time
		dev.delay_us(70000, NULL); // Adjust based on your configuration

		rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
		if (rslt == BME280_OK) {
			// Successfully read the sensor data, process it (e.g., log or transmit)
			printf("Temp: %.2f, Press: %.2f, Hum: %.2f\n", comp_data.temperature, comp_data.pressure, comp_data.humidity);
			} else {
			// Handle error
		}

		// Implement a delay or use a timer to read data at a desired interval
	}

	return 0;
}




BME280_INTF_RET_TYPE user_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
	// Set CS low
	cs_select();
	// Send reg_addr with read bit
	reg_addr = reg_addr | 0x80; // Assuming the MSB is the read bit
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor((struct spi_m_sync_descriptor *)intf_ptr, &io);
	spi_m_sync_enable((struct spi_m_sync_descriptor *)intf_ptr);

	// Send reg_addr with read bit
	io_write(io, &reg_addr, 1);
	
	// Read len bytes of data into reg_data
	io_read(io, reg_data, len);
	
	// Set CS high
	cs_deselect();
	return 0; // Return 0 for success
}

BME280_INTF_RET_TYPE user_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
	cs_select(); // Set CS low
	
	// Clear the read bit for write operation
	reg_addr = reg_addr & 0x7F; // Assuming the MSB needs to be cleared for write
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor((struct spi_m_sync_descriptor *)intf_ptr, &io);
	spi_m_sync_enable((struct spi_m_sync_descriptor *)intf_ptr);

	// Send reg_addr with write bit
	io_write(io, &reg_addr, 1);
	
	// Write len bytes of data from reg_data
	io_write(io, reg_data, len);
	
	cs_deselect(); // Set CS high
	return 0; // Return 0 for success
}

void user_delay_us(uint32_t period, void *intf_ptr) {
	// Implement your delay function here, delay for 'period' microseconds
}



// Example functions for setting the CS pin state
void cs_select() {
	// Sets the Humidity_CS pin low
	gpio_set_pin_level(Humidity_CS,false)
	
}

void cs_deselect() {
	// Sets the Humidity_CS pin high
	gpio_set_pin_level(Humidity_CS, true);
}


void SPI_write(uint8_t *buffer, size_t length) {
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	spi_m_sync_enable(&SPI_0);
	io_write(io, buffer, length); // Correctly pass the buffer and its length
}

	
uint8_t SPI_read_byte() {
	struct io_descriptor *io;
    spi_m_sync_get_io_descriptor(&SPI_0, &io);

    spi_m_sync_enable(&SPI_0);
    uint8_t buffer[1] = {0}; // Initialize buffer
    io_read(io, buffer, 1);
    return buffer[0]; // Return the read byte
}


/*
 * CFile1.c
 *
 * Created: 1/31/2024 3:22:06 PM
 *  Author: tomas
 */ 

#include "bme280.h" // Include the BME280 driver
// Include other necessary standard and microcontroller-specific headers
// For example:
#include <stdio.h>
#include "samv71_spi.h" // Placeholder for actual SPI library
#include "samv71_gpio.h" // Placeholder for actual GPIO library

#define BME280_CS_PIN PD25

BME280_INTF_RET_TYPE spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
	// Activate CS pin
	gpio_set_pin_low(BME280_CS_PIN);
	
	// Send the register address with the read command
	spi_transfer(reg_addr | 0x80); // Assuming 0x80 is the read command for your SPI
	// Read data
	for (uint32_t i = 0; i < len; i++) {
		reg_data[i] = spi_transfer(0); // Send dummy data to read
	}
	
	// Deactivate CS pin
	gpio_set_pin_high(BME280_CS_PIN);
	
	return BME280_INTF_RET_SUCCESS;
}

BME280_INTF_RET_TYPE spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
	// Activate CS pin
	gpio_set_pin_low(BME280_CS_PIN);
	
	// Send the register address with the write command
	spi_transfer(reg_addr & 0x7F); // Assuming MSB 0 is the write command for your SPI
	
	// Write data
	for (uint32_t i = 0; i < len; i++) {
		spi_transfer(reg_data[i]);
	}
	
	// Deactivate CS pin
	gpio_set_pin_high(BME280_CS_PIN);
	
	return BME280_INTF_RET_SUCCESS;
}

void delay_us(uint32_t period, void *intf_ptr) {
	// Implement a microsecond delay
}


int main(void) {
	struct bme280_dev dev;
	int8_t rslt = BME280_OK;

	// Assign function pointers to the BME280 driver
	dev.read = spi_read;
	dev.write = spi_write;
	dev.delay_us = delay_us;
	dev.intf_ptr = NULL; // If needed
	dev.intf = BME280_SPI_INTF;

	// Initialize the BME280
	rslt = bme280_init(&dev);

	if (rslt != BME280_OK) {
		printf("Failed to initialize the device.\n");
		return -1;
	}

	// Further steps to read sensor data...
}


// Assuming this part is inside the main function after initializing the BME280

struct bme280_data comp_data;

// Set sensor settings and modes as required
// For example, set to normal mode and set oversampling
dev.settings.osr_h = BME280_OVERSAMPLING_1X;
dev.settings.osr_p = BME280_OVERSAMPLING_1X;
dev.settings.osr_t = BME280_OVERSAMPLING_1X;
dev.settings.filter = BME280_FILTER_COEFF_2;

uint8_t settings_sel = BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM | BME280_SEL_FILTER;
rslt = bme280_set_sensor_settings(settings_sel, &dev);

// Set the sensor to normal mode
rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &dev);

// Read sensor data
rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);

if (rslt == BME280_OK) {
	printf("Temp: %0.2f, Press: %0.2f, Hum: %0.2f\n",
	comp_data.temperature, comp_data.pressure, comp_data.humidity);
	} else {
	printf("Failed to read sensor data\n");
}



//--------------------

#include "bme280.h" // BME280 driver

// Placeholder for actual SPI and GPIO initialization functions
void spi_init(void);
void gpio_init(void);

// Define the CS pin based on your connection
#define BME280_CS_PIN PD25

// Function to initialize BME280
int8_t bme280_simple_init(struct bme280_dev *dev);

int main(void) {
	struct bme280_dev dev;
	int8_t rslt;

	// Initialize SPI and GPIO (This is very platform specific)
	spi_init();
	gpio_init();

	// Initialize BME280
	rslt = bme280_simple_init(&dev);
	if (rslt == BME280_OK) {
		printf("BME280 is responding.\n");
		} else {
		printf("Failed to initialize BME280.\n");
	}

	// Further steps could involve reading and printing the actual data
	// For this simple example, we're only checking if the sensor is responding

	return 0;
}

// Simple BME280 initialization function
int8_t bme280_simple_init(struct bme280_dev *dev) {
	int8_t rslt = BME280_OK;

	dev->intf = BME280_SPI_INTF;
	dev->read = spi_read;  // Assign your SPI read function
	dev->write = spi_write;  // Assign your SPI write function
	dev->delay_us = delay_us;  // Assign your delay function

	// Initialize the BME280
	rslt = bme280_init(dev);

	return rslt;
}

// Dummy placeholder functions for SPI read/write and delay
BME280_INTF_RET_TYPE spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
	// Your SPI read implementation
	return BME280_INTF_RET_SUCCESS;
}

BME280_INTF_RET_TYPE spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
	// Your SPI write implementation
	return BME280_INTF_RET_SUCCESS;
}

void delay_us(uint32_t period, void *intf_ptr) {
	// Your delay implementation
}


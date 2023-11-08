/*
 * CFile1.c
 *
 * Created: 11/8/2023 12:31:21 PM
 *  Author: 10032
 */ 

#include "bme280_sensor.h"
#include "bme280.h" // Include the BME280 driver header

// Assuming you have a BME280 driver that provides these functions
extern struct bme280_dev bme280_device;

void bme280_sensor_init(void) {
	// Initialization code for the BME280 sensor
	bme280_init(&bme280_device);
}

void bme280_start_measurement(void) {
	// Code to start measurement
	bme280_set_sensor_mode(BME280_FORCED_MODE, &bme280_device);
}

void bme280_stop_measurement(void) {
	// Code to stop measurement
}

void bme280_read_data(uint32_t *temperature, uint32_t *pressure, uint32_t *humidity) {
	struct bme280_data comp_data;
	bme280_get_sensor_data(BME280_ALL, &comp_data, &bme280_device);

	// Assign data to the pointers
	*temperature = comp_data.temperature;
	*pressure = comp_data.pressure;
	*humidity = comp_data.humidity;
}
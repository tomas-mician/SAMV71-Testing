/*
 * CFile1.c
 *
 * Created: 11/8/2023 12:33:22 PM
 *  Author: 10032
 */ 

#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include <stdint.h>

void bme280_sensor_init(void);
void bme280_start_measurement(void);
void bme280_stop_measurement(void);
void bme280_read_data(uint32_t *temperature, uint32_t *pressure, uint32_t *humidity);

#endif // BME280_SENSOR_H


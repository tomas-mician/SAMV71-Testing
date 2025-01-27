	#include <atmel_start.h>
//#include <asf.h>
#include "variables.h"
#include <stdint.h>
#include <string.h>
#include "buffer_queue.h"
//#include "bme280_sensor.h"

#include "bme280.h"
//#include "SPI.h" // Replace with your SPI HAL header
//#include "gpio.h" // Replace with your GPIO HAL header

// SPI and GPIO configurations (adjust these according to your setup)
#define SPI_MODULE     SPI_0


struct bme280_dev bme280; // BME280 device structure

// Commands
// 0 idle report
// 1 send bin
// 2 send event-by-event
#define INITIAL_DATA_MODE 1

volatile uint8_t data_mode = INITIAL_DATA_MODE;
volatile uint8_t previous_data_mode = INITIAL_DATA_MODE;
int rt = 1;

#define NUM_OF_BINS 16
volatile uint32_t energy_bins[NUM_OF_DETECTOR][NUM_OF_BINS];

#define EVENT_MESSAGE_LENGTH 13
#define BIN_MESSAGE_LENGTH 7+NUM_OF_BINS*4

volatile uint8_t message_interval_ms = 10;
#define STARTBYTE 'S'


// Add buffer queue for bin and event modes
extern BinBufferQueue bin_buffer_queue;
extern EventBufferQueue event_buffer_queue;


// struct for Timer Task
static struct timer_task task;
static struct timer_task micro_task;
uint16_t milliCounter = 0;
uint32_t secondCounter = 0;
uint16_t microCounter = 0;

// #define CS_PIN_DEVICE1 GPIO(GPIO_PORTD, 25) // replace with your actual CS pins
#define CS_PIN_DEVICE1 PIO_PD25_IDX
#define CS_PIN_DEVICE2 PIO_PA29_IDX
#define CS_PIN_DEVICE3 PIO_PA30_IDX
#define CS_PIN_DEVICE4 PIO_PA31_IDX

int calibrated_time_ms = 0;
int time_datum = 0;
uint8_t startSend = 0;
#define QUEUE_SIZE 60000

// buffer for data passing down
volatile uint8_t detector_data[NUM_OF_DETECTOR][DATA_LENGTH];

// create a ring buffer for an efficient rolling buffer
int end_index = 0;
int start_index = 0;


// Serial set up
// Serial receiving & Complete Flags
volatile uint8_t serial_receiving = 0;
volatile uint8_t serial_complete = 0;
volatile uint8_t send_data_flag = 0;
uint32_t messageCounter = 0;


// Humidity sensor reading flag
volatile bool read_bme280_flag = false;



// Bytes Received Counters
volatile uint8_t serial_received_bytes_counter = 0;
volatile uint8_t total_bytes = 0;





/**
* Callback for Timer Task
*
*/
static void timer_task_cb(const struct timer_task *const timer_task)
{
	// Toggle LED
	milliCounter++;
	
	messageCounter++;

	// Check if it's time to send a message
	if (messageCounter >= message_interval_ms) {
		// Send serial message
		send_data_flag = 1;
		// Reset message counter
		messageCounter = 0;
	}

	// Reset millisecond counter every second
	if (milliCounter >= 1000) {
		milliCounter = 0;
		secondCounter++;
		

		read_bme280_flag = true;

	}
	
	
}

static void micro_timer_task_cb(const struct timer_task *const timer_task)
{
	microCounter++;
	// Reset every millisecond
	if (microCounter >= 1000) {
		microCounter = 0;
	}
}


void process_detector_data(uint8_t new_entry, uint8_t detector_index) {
	// Assuming new_entry is the output of ADC (0 - 65535)
	// Map this to one of your 16 bins
	int bin_index = new_entry / (65536 / NUM_OF_BINS);
	
	// Increase the count for this bin
	energy_bins[detector_index][bin_index]++;
}


// SPI functions

void read_SPI_data(void)
{
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	
	uint8_t read_data; // Array to hold the read data

	// List of CS pins for all devices
	uint8_t cs_pins[4] = {CS_PIN_DEVICE1, CS_PIN_DEVICE2, CS_PIN_DEVICE3, CS_PIN_DEVICE4};
	
	

	for (int i = 0; i < 4; i++) {
		

		gpio_set_pin_level(cs_pins[i], false); // set the pin low (select the device)
		io_read(io, &read_data, 1); // Read 1 bytes of data
		gpio_set_pin_level(cs_pins[i], true); // set the pin high (de-select the device)
		add_to_buffer(read_data, i);
		
		process_detector_data(read_data,i);
		
	}
}


// buffer functions
void add_to_buffer(uint8_t new_entry, uint8_t detector_id) {
	detector_data[detector_id][end_index] = new_entry;
	end_index = (end_index + 1) % DATA_LENGTH;
	
	// If the buffer is full, increment the start index as well
	if (end_index == start_index) {
		start_index = (start_index + 1) % DATA_LENGTH;
	}
	return;
}

uint8_t get_from_buffer(uint8_t detector_id) {
	if (start_index == end_index) {
		// Buffer is empty, return an error value
		return 0xFF;
	}
	
	uint8_t entry = detector_data[detector_id][start_index];
	start_index = (start_index + 1) % DATA_LENGTH;
	return entry;
}

// USART functions
// Virtual COM port transmit callback function
static void serial_tx_cb(const struct usart_async_descriptor *const io_descr) {
	
	// Do nothing so far
}

#define WAITING_FOR_START 0
#define WAITING_FOR_MODE 1
static uint8_t receive_state = WAITING_FOR_START;
// When serial reads a data
static void serial_rx_cb(const struct usart_async_descriptor *const io_descr, const uint16_t usart_data) {
	uint8_t received_byte, count;
	
	count = io_read(&USART_0, &received_byte,1);

    switch (receive_state) {
        case WAITING_FOR_START:
            if (received_byte == 'S') {
                receive_state = WAITING_FOR_MODE;
            }
            break;
        case WAITING_FOR_MODE:
            data_mode = received_byte;
            receive_state = WAITING_FOR_START;  // Ready to receive next message
			startSend = 1;
            break;
    }
	
}

void data_mode_check(uint8_t data_mode, uint8_t previous_data_mode) {
	if (data_mode != previous_data_mode) {
		if (data_mode == 2) {
			// Enable microsecond timer
			timer_start(&MICRO_Timer);
			} else {
			// Disable microsecond timer
			timer_stop(&MICRO_Timer);
		}
		previous_data_mode = data_mode;  // Update the previous data mode
	}
}


void serial_send_data() {
}




struct bme280_dev bme280;

// SPI read and write functions
/*
int8_t spi_reg_write(uint8_t cs, uint8_t reg_addr, uint8_t *reg_data, uint16_t length) {
	gpio_set(CS_GPIO_PORT, CS_GPIO_PIN, GPIO_PIN_RESET); // Activate CS
	spi_transfer(SPI_MODULE, &reg_addr, 1, NULL, 0); // Write address
	spi_transfer(SPI_MODULE, reg_data, length, NULL, 0); // Write data
	gpio_set(CS_GPIO_PORT, CS_GPIO_PIN, GPIO_PIN_SET); // Deactivate CS
	return 0;
}

int8_t spi_reg_read(uint8_t cs, uint8_t reg_addr, uint8_t *reg_data, uint16_t length) {
	reg_addr = 0x80 | reg_addr; // Set read bit
	gpio_set(CS_GPIO_PORT, CS_GPIO_PIN, GPIO_PIN_RESET); // Activate CS
	spi_transfer(SPI_MODULE, &reg_addr, 1, NULL, 0); // Write address
	spi_transfer(SPI_MODULE, NULL, 0, reg_data, length); // Read data
	gpio_set(CS_GPIO_PORT, CS_GPIO_PIN, GPIO_PIN_SET); // Deactivate CS
	return 0;
}
*/

void spi_init(void) {
	// Initialize SPI module
	spi_m_sync_init(&SPI_0, SPI0);

	// Set SPI baud rate, mode, etc. as needed
	spi_m_sync_set_baudrate(&SPI_0, 115200); // Example: 1 MHz. Changed to 115200 for now
	spi_m_sync_enable(&SPI_0);
}


void gpio_init(void) {
	// Initialize CS_PIN_DEVICE1 as output and set it high
	gpio_set_pin_direction(CS_PIN_DEVICE1, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(CS_PIN_DEVICE1, true);
}


int8_t spi_reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint16_t length) {
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	spi_m_sync_enable(&SPI_0);

	// Activate CS
	gpio_set_pin_level(CS_PIN_DEVICE1, false);

	// Write register address
	io_write(io, &reg_addr, 1);

	// Write data
	io_write(io, reg_data, length);

	// Deactivate CS
	gpio_set_pin_level(CS_PIN_DEVICE1, true);

	return 0; // Indicate success
}



int8_t spi_reg_read(uint8_t reg_addr, uint8_t *reg_data, uint16_t length) {
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	spi_m_sync_enable(&SPI_0);

	reg_addr |= 0x80; // Set the MSB for read operation

	// Activate CS
	gpio_set_pin_level(CS_PIN_DEVICE1, false);

	// Write register address
	io_write(io, &reg_addr, 1);

	// Read data
	io_read(io, reg_data, length);

	// Deactivate CS
	gpio_set_pin_level(CS_PIN_DEVICE1, true);

	return 0; // Indicate success
}





// Delay function
void user_delay_ms(uint32_t period) {
	// Implement a delay function suitable for your platform
}


void initialize_bme280() {
	// Assign device structure parameters
	//bme280.dev_id = 0;
	bme280.intf = BME280_SPI_INTF;
	bme280.read = spi_reg_read;
	bme280.write = spi_reg_write;
	bme280.delay_us = user_delay_ms;

	// Initialize the BME280
	bme280_init(&bme280);
}


void read_sensor_data() {
	struct bme280_data comp_data;
	int8_t rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &bme280);

	if (rslt == BME280_OK) {
		// Format and send sensor data over UART
		char buffer[128];
		snprintf(buffer, sizeof(buffer), "Temperature: %0.2f C, Pressure: %0.2f hPa, Humidity: %0.2f%%\r\n",
		comp_data.temperature, comp_data.pressure / 100.0, comp_data.humidity);
		io_write(&USART_0.io, (uint8_t *)buffer, strlen(buffer));
		} else {
		// Handle error
		// For example, send an error message over UART
		char error_msg[] = "Error reading BME280 sensor data\r\n";
		io_write(&USART_0.io, (uint8_t *)error_msg, strlen(error_msg));
	}
}

int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();
	
	// Initialize SPI and GPIO (implement these functions according to your HAL)
	spi_init(); //was SPI_MODULE
	gpio_init(); // choose the pin and port

	// Initialize BME280
	initialize_bme280();

	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	spi_m_sync_enable(&SPI_0);

	uint8_t read_data[12]; // Buffer to store the data that is read
	
	
	// NOTE: we might not need this dev as the bme_280 struct was already globally initialized elsewhere in main.c
	 struct bme280_dev dev;
	 struct bme280_data comp_data;

 // BME280 sensor initialization
 // Make sure to implement the user I2C or SPI read/write and delay functions
//  bme280.dev_id = BME280_I2C_ADDR_PRIM; // Or BME280_I2C_ADDR_SEC
//  bme280.intf = BME280_I2C_INTF;
//  bme280.read = user_i2c_read; // Replace with actual I2C read function
//  bme280.write = user_i2c_write; // Replace with actual I2C write function
//  bme280.delay_ms = user_delay_ms; // Replace with actual delay function

 //bme280_init(&bme280); [recent]
	
	// Set up Timer Function
	task.interval = 1;
	task.cb = timer_task_cb;
	task.mode = TIMER_TASK_REPEAT;
	
	// Add timer task
	timer_add_task(&TIMER_0, &task);
	timer_start(&TIMER_0);
	
	// Set up Microsecond Timer Function
	micro_task.interval = 1;
	micro_task.cb = micro_timer_task_cb;
	micro_task.mode = TIMER_TASK_REPEAT;
	
	timer_add_task(&MICRO_Timer, &micro_task);
	if (data_mode != previous_data_mode) {
		if (data_mode == 2) {
			// Enable microsecond timer
			timer_start(&MICRO_Timer);
			} else {
			// Disable microsecond timer
			timer_stop(&MICRO_Timer);
		}
		previous_data_mode = data_mode;  // Update the previous data mode
	}


	// Configure CS pins as output, initial state high
	gpio_set_pin_direction(CS_PIN_DEVICE1, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE2, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE3, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE4, GPIO_DIRECTION_OUT);
	
	// Enable SPI
	spi_m_sync_enable(&SPI_0);
	
	// 
	usart_async_register_callback(&USART_0, USART_ASYNC_TXC_CB, serial_tx_cb);
	usart_async_register_callback(&USART_0, USART_ASYNC_RXC_CB, serial_rx_cb);
	int32_t result = usart_async_enable(&USART_0);
	if (result == ERR_NONE) {
		uint8_t startPrint [12] = "Serial ready";
		uint8_t newline[] = "\n";
		//io_write(&USART_0.io, startPrint, 12);
		//io_write(&USART_0.io, newline, sizeof(newline));
		memset(startPrint,0x00,12);
		memset(newline,0x00,sizeof(newline));
	}
	


	while (1) {
		
		// Example of reading sensor data
		// if (some_condition_to_read_sensor) {
		// 	uint32_t temperature, pressure, humidity;
		// 	bme280_read_data(&temperature, &pressure, &humidity);
		 //	Use the sensor data as needed
		//}
			
		read_SPI_data();
		
		
		     if (read_bme280_flag) {
					read_sensor_data(); // Should handle all the reading making the next code redundant
					
					/*
					struct bme280_data comp_data;
					int8_t rslt = bme280_get_sensor_data(BME280_HUM, &comp_data, &bme280);
					if (rslt == BME280_OK) {
						// Successfully read humidity data
						// Process the humidity data as required
						// Example: Sending over USART
						char buffer[64];
						snprintf(buffer, sizeof(buffer), "Humidity: %0.2f %%\r\n", comp_data.humidity);
						io_write(&USART_0.io, (uint8_t*)buffer, strlen(buffer));
					} 
					*/

			      

			       // Read the sensor data
				   //io_read(io, read_data, 12); // Read 12 bytes of data into read_data buffer
				       // Format the sensor data into a string
				      

				      // Send the string over UART
				    //io_write(&USART_0, read_data,sizeof(read_data));
				    
					   
				   
				    read_bme280_flag = false;
		       }
		
		if (send_data_flag) {
			
			switch (data_mode) {
				case 0: // idle
				
				break;
				
				case 1: // bin mode
				{
					BinBufferItem item;
					item.mode = data_mode;
					item.secondCounter = secondCounter;
					item.milliCounter = milliCounter;

					// Add bin data to the buffer
					for (int i = 0; i < NUM_OF_DETECTOR; i++) {
						for (int j = 0; j < NUM_OF_ENERGY_LEVELS; j++) {
							item.data[i * NUM_OF_ENERGY_LEVELS + j] = energy_bins[i][j];
						}
					}
					
					// Reset the bins after sending data
					memset(energy_bins, 0, NUM_OF_DETECTOR * NUM_OF_ENERGY_LEVELS * sizeof(uint8_t));
					
					// enqueue the buffer
					bin_buffer_enqueue(item);
					break;
				}
				
				case 2: // event by event
				{
					EventBufferItem item;
					item.mode = data_mode;
					item.secondCounter = secondCounter;
					item.milliCounter = milliCounter;
					item.microCounter = microCounter;

					// add detector data to the buffer
					for (int i = 0; i < NUM_OF_DETECTOR; i++) {
						item.data[i] = get_from_buffer(i);
					}

					// enqueue the buffer
					event_buffer_enqueue(item);
					break;
				}
				
				default:
				break;
				
			}


			//uint8_t testSend = "ter";
			//BinBufferItem dequeueItemBinToCopy = bin_buffer_dequeue();
			//uint8_t dequeueItemBin[sizeof(dequeueItemBinToCopy)]; 
			////fprintf(sizeof(uint32_t));
			//memcpy(&dequeueItemBin,&dequeueItemBinToCopy,sizeof(dequeueItemBinToCopy));
			//EventBufferItem dequeueItemEvent = event_buffer_dequeue();
				////uint8_t dequeueItemEvent[sizeof(dequeueItemEventToCopy)]; 
			////fprintf(sizeof(uint32_t));
			////memcpy(&dequeueItemEvent,&dequeueItemEventToCopy,sizeof(dequeueItemEventToCopy));
			////int result = io_write(&USART_0.io, (uint8_t *)&dequeueItemBin, sizeof(dequeueItemBin));
			//int result = io_write(&USART_0.io, (uint8_t *)&dequeueItemEvent, sizeof(dequeueItemEvent));
						
			if (startSend == 1) {
				switch (data_mode) {
					
					case 0:
					break;
					
					case 1:
					{
						BinBufferItem dequeueItemBin = bin_buffer_dequeue();
						int result = io_write(&USART_0.io, (uint8_t *)&dequeueItemBin, sizeof(dequeueItemBin));
						break;
					}
					case 2:
					{
						EventBufferItem dequeueItemEvent = event_buffer_dequeue();
						int result = io_write(&USART_0.io, (uint8_t *)&dequeueItemEvent, sizeof(dequeueItemEvent));
						break;
					}
					default:
					break;
				}
			}
			
			send_data_flag = 0;
			
		}
			 }

		
	}

// Put a pulse in and see how fast that will go - HOW SHORT CAN WE MAKE THE PULSE - OSCILLOSCOPE HESSE
// We need to measure the width of upper discriminator
// idea: timer interrupt, every time it interrupts send an input signal with given width??
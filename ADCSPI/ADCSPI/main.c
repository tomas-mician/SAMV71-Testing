#include <atmel_start.h>
//#include <asf.h>
#include "variables.h"
#include <stdint.h>
#include <string.h>




// Commands
// 0 send bin
// 1 request for time
// 2 send event-by-event

volatile uint8_t data_mode = 1;

#define MESSAGE_LENGTH 13

#define MESSAGE_INTERVAL_MS 30
#define STARTBYTE 'S'



// struct for Timer Task
static struct timer_task task;
uint16_t milliCounter = 0;
uint32_t secondCounter = 0;

#define CS_PIN_DEVICE1 PIO_PA28_IDX // replace with your actual CS pins
#define CS_PIN_DEVICE2 PIO_PA29_IDX
#define CS_PIN_DEVICE3 PIO_PA30_IDX
#define CS_PIN_DEVICE4 PIO_PA31_IDX

int calibrated_time_ms = 0;
int time_datum = 0; 
uint8_t startSend = 0;


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



// Bytes Received Counters
volatile uint8_t serial_received_bytes_counter = 0;
volatile uint8_t total_bytes = 0;

// Size of receive buffer
#define SERIAL_BUFFER_SIZE 200

// Receive and transmit buffers
volatile uint8_t rx_buffer[SERIAL_BUFFER_SIZE] = { 0x00 }; // make sure we don't have anything left in the memory
volatile uint8_t tx_buffer[SERIAL_BUFFER_SIZE + 14] = "Your message: ";
volatile uint8_t indicator_buffer[3] = "YES";




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
	if (messageCounter >= MESSAGE_INTERVAL_MS) {
		// Send serial message
		send_data_flag = 1;
		// Reset message counter
		messageCounter = 0;
	}

	// Reset millisecond counter every second
	if (milliCounter >= 1000) {
		milliCounter = 0;
		secondCounter++;
	}
	
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
		gpio_set_pin_level(cs_pins[i], true); // set the pin high (deselect the device)
		add_to_buffer(read_data, i);
		

		//data_device = (read_data[0]<<8) | read_data[1]; // Assume the data is in big-endian byte order
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


// When serial reads a data
static void serial_rx_cb(const struct usart_async_descriptor *const io_descr, const uint16_t usart_data) {
		uint8_t received_byte, count;
		received_byte = 0;
		
		count = io_read(&USART_0.io, &received_byte, 1);
		if (received_byte == STARTBYTE) {
			startSend = 1;
		}
		
	
}

void serial_send_data() {
}


int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	
	// Set up Timer Function
	task.interval = 1;
	task.cb = timer_task_cb;
	task.mode = TIMER_TASK_REPEAT;
	
	// Add timer task
	timer_add_task(&TIMER_0, &task);
	timer_start(&TIMER_0);


	// Configure CS pins as output, initial state high
	gpio_set_pin_direction(CS_PIN_DEVICE1, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE2, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE3, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE4, GPIO_DIRECTION_OUT);
	
	// Enable SPI
	spi_m_sync_enable(&SPI_0);
	
	
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
		read_SPI_data();
		
		if (send_data_flag) {
			
			uint8_t buffer[1 + sizeof(secondCounter) + sizeof(milliCounter) + NUM_OF_DETECTOR] = {0x00};

			int index = 0;
			volatile int result = 0;
			

			// add data_mode to the buffer
			buffer[index++] = data_mode;

			// add secondCounter to the buffer
			memcpy(&buffer[index], &secondCounter, sizeof(secondCounter));
			index += sizeof(secondCounter);

			// add milliCounter to the buffer
			memcpy(&buffer[index], &milliCounter, sizeof(milliCounter));
			index += sizeof(milliCounter);

			// add detector data to the buffer
			for (int i = 0; i < NUM_OF_DETECTOR; i++) {
				uint8_t detector = get_from_buffer(i);
				buffer[index++] = detector;
			}
		

			// write the entire buffer
			
			if (startSend == 1) {
				result = io_write(&USART_0.io, &buffer, sizeof(buffer));
			}
			
			memset(&buffer, 0x00,sizeof(buffer));
			send_data_flag = 0;
			}

		
	}
}
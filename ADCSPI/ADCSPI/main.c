#include <atmel_start.h>

#define CS_PIN_DEVICE1 PIO_PA28_IDX // replace with your actual CS pins
#define CS_PIN_DEVICE2 PIO_PA29_IDX
#define CS_PIN_DEVICE3 PIO_PA30_IDX
#define CS_PIN_DEVICE4 PIO_PA31_IDX


// Serial set up 
// Serial receiving & Complete Flags
volatile uint8_t serial_receiving = 0;
volatile uint8_t serial_complete = 0;


// Bytes Received Counters
volatile uint8_t serial_received_bytes_counter = 0;
volatile uint8_t total_bytes = 0;

// Size of receive buffer
#define SERIAL_BUFFER_SIZE 200

// Receive and transmit buffers
volatile uint8_t rx_buffer[SERIAL_BUFFER_SIZE] = { 0x00 }; // make sure we don't have anything left in the memory
volatile uint8_t tx_buffer[SERIAL_BUFFER_SIZE + 14] = "Your message: ";
volatile uint8_t indicator_buffer[3] = "YES";


// SPI functions

void read_SPI_data(void)
{
	struct io_descriptor *io;
	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	
	uint8_t read_data[2]; // Array to hold the read data
	uint16_t data_device; // Variable to store the final integer value

	// List of CS pins for all devices
	uint8_t cs_pins[4] = {CS_PIN_DEVICE1, CS_PIN_DEVICE2, CS_PIN_DEVICE3, CS_PIN_DEVICE4};

	for (int i = 0; i < 4; i++) {
		

		gpio_set_pin_level(cs_pins[i], false); // set the pin low (select the device)
		io_read(io, read_data, 2); // Read 2 bytes of data
		gpio_set_pin_level(cs_pins[i], true); // set the pin high (deselect the device)

		data_device = (read_data[0]<<8) | read_data[1]; // Assume the data is in big-endian byte order

		// Use or store the data as needed
		// ...
	}
}

// USART functions
// Virtual COM port transmit callback function
static void serial_tx_cb(const struct usart_async_descriptor *const io_descr) {
	
	// Do nothing so far
}


int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	// Configure CS pins as output, initial state high
	gpio_set_pin_direction(CS_PIN_DEVICE1, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE2, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE3, GPIO_DIRECTION_OUT);
	gpio_set_pin_direction(CS_PIN_DEVICE4, GPIO_DIRECTION_OUT);
	
	// Enable SPI
	spi_m_sync_enable(&SPI_0);

	while (1) {
		read_SPI_data();
		
		// Add any other main loop code here
		// ...
	}
}


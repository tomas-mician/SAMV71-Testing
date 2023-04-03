#include <atmel_start.h>

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
 
 // Call back function for receiving data
 static void serial_rx_cb(const struct usart_async_descriptor *const io_descr) {
	 // Counters
	 uint8_t ch, count;
	 
	 // Read a character in
	 count = io_read(&USART_0.io, &ch, 1);
	 
	 // Check if we are receiving
	 if (serial_receiving == 0) {
		 // Check for new lines or carriage return
		 if (ch != '\r' && ch != '\n') {
			 serial_receiving = 1;
		 
		 // Reset byte counter
		 serial_received_bytes_counter = 0;
		 
		 // Start filling the buffer
		 rx_buffer[serial_received_bytes_counter] = ch;
		 
		 // Increment the byte counter
		 serial_received_bytes_counter += count;
		 }
	 }
	 else 
	 {
		 // Continue filling buffer
		 rx_buffer[serial_received_bytes_counter] = ch;
		 
		 // Increment the counter
		 serial_received_bytes_counter += count;
		 
		 // Check for new line or carriage return
		 if (ch == '\r' || ch == '\n')
		 {
			 // Set the completion flag
			 serial_complete = 1;
			 
			 // Total bytes
			 // since the terminating characters ('\r' or '\n') take 2 characters, we will eliminate them from the total count
			 total_bytes = serial_received_bytes_counter - 2; 
		 }
		 
		 // Check for buffer overflow
		 // if overflow, starts overwriting
		 if (serial_received_bytes_counter == SERIAL_BUFFER_SIZE)
		 {
			 serial_received_bytes_counter = 0;
		 }
	 }
	 	 
		 
 }
 
 
 // Virtual COM port transmit callback function
 static void serial_tx_cb(const struct usart_async_descriptor *const io_descr) {
	 
	 // Do nothing so far
 }
 
 
int main(void)
{
	/* Initializes MCU, drivers */
	atmel_start_init();
	
	// Initialize Async drivers
	usart_async_register_callback(&USART_0, USART_ASYNC_TXC_CB, serial_tx_cb);
	usart_async_register_callback(&USART_0, USART_ASYNC_RXC_CB, serial_rx_cb);
	usart_async_enable(&USART_0);
	
	uint8_t idlebuffer [12] = "start typing";
	io_write(&USART_0.io, idlebuffer, 12);
			// Clear memory
			memset(&idlebuffer, 0x00, 12);

	while (1) {
		
		// Check if receiving
		if (serial_receiving == 1) {
			
			// Check if receive is complete
			if (serial_complete == 1) {
				// Reset flags
				serial_complete = 0;
				serial_receiving = 0;
				
				// Copy message to TX Buffer
				memcpy(&tx_buffer[14], &rx_buffer[0], SERIAL_BUFFER_SIZE);
				
				// Print the message to console
				// 16 = 14 + 2, the end of the message '\r' or '\n'
				io_write(&USART_0.io, tx_buffer, total_bytes + 16);
				//io_write(&USART_0.io, indicator_buffer, 3);
				
				// Clear memory
				memset(&rx_buffer, 0x00, SERIAL_BUFFER_SIZE);
			}
			//else {
				//uint8_t idlebuffer [7] = "Hello\n";
				//io_write(&USART_0.io, idlebuffer, 7);
				//// Clear memory
				//memset(&idlebuffer, 0x00, 7);
			//}
		}
		else {
			//uint8_t idlebuffer [7] = "Hello\n";
			//io_write(&USART_0.io, idlebuffer, 7);
			//// Clear memory
			//memset(&idlebuffer, 0x00, 7);
		}
	
	}
}



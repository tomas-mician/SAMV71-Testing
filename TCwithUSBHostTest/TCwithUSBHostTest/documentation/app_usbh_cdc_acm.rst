================
USB CDC ACM Demo
================

This demo enumerates a USB CDC ACM serial enumeration device and perform tests
on it.

User can interact through a terminal connected to the board by EDBG COM port.
The communication baud rate for the COM port is 9600 bps.

The demo enumerates single or first two of CDC ACM serial functions on a USB
device, open one of the serial port and accept user inputs to send data blocks
to connected device. When any data is received from device, it dumps received
string to COM port. Refer to demo hints on COM port for more information.

Drivers
-------
* USB Host
* USART (for STDIO)

Middleware
----------
* USB Host Core
* USB Host CDC ACM
* STDIO

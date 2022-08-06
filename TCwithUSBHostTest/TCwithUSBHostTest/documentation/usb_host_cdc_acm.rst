================
USB Host CDC ACM
================

USB Host CDC ACM is a part of the USB Host Stack library. It provides support
for USB CDC ACM Function driver implementation. It enumerates and handles
inputs from USB CDC ACM function/interface. For more detailed definition and
description about this class, user can refer to
<Universal Serial Bus Class Definitions for Communication Devices, Vision 1.1>.
**Section 3.6.2 Abstract Control Model** is for CDC ACM.

The driver is registered to the USB Host Core Driver, so that when USB CDC ACM
serial Device or USB Composite Device with CDC ACM serial Function is connected,
it can be enabled to drive the CDC ACM function.

The open/close and read/write APIs are not blocked, that is, the functions are
returned immediately, when port state changed or transfer is done callbacks will
be invoked. Also status checking functions are available to check if background
operation is done.

Timeout is always applied to read/write operations, if there is no successful
transactions (ACK) for specific time, timeout will happen and number of bytes
transfered is reported in transfer end callback.

For USB transfer, if the transfer size is exactly the multiple of USB endpoint
max packet size, the transfer will not be terminated before zero length packet
(ZLP) is sent, in this case if user confirms packet ending, just call write
flush function to terminate transfer.

Features
--------

* Initialization/de-initialization.
* Open/close of CDC ACM serial port.
* Read/write on opened CDC ACM serial port.
* Notifications on state change for CDC ACM serial port.
* Notifications on read/write end for CDC ACM serial port.

Applications
------------

* Detect, enumerate and operate CDC ACM Serial Enumeration Device.

Dependencies
------------

* USB Host Driver
* USB Host Stack Core
* USB Protocol CDC

Limitations
-----------

N/A




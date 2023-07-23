import serial
import base64
from serial.tools import list_ports


# List available COM ports
ports = list(list_ports.comports())
for i, port in enumerate(ports):
    print(f"{i+1}. {port}")

# Ask the user to input the COM port number
port_num = int(input("Please enter the number of the COM port you want to use: ")) - 1
port = ports[port_num].device

# Open serial port
ser = serial.Serial(port, 9600)  # Change 'COM5' to your port name, 9600 to your baud rate


startByte = b'S'; 
endByte = b'E';

# Byte order
order = 'little'


data = b'\x00'


# Main loop to start and end reading data
while True:

    # Read 1 bytes
    data = ser.read(1)

    if data == startByte:
        print(data)

        # Unpack byte to int
        int_value = int.from_bytes(data, byteorder=order, signed=False)

        # Check for start number
        if int_value == startNum:
            # Now read the data

            # read 1 byte
            data_mode = int.from_bytes(ser.read(1), byteorder=order, signed=False)
            
            # read 4 bytes
            secondCounter = int.from_bytes(ser.read(4), byteorder=order, signed=False)
            
            # read 4 bytes
            milliCounter = int.from_bytes(ser.read(4), byteorder=order, signed=False)
            
            # read 4 bytes for detector data
            detectors = []
            for i in range(4):
                detector = int.from_bytes(ser.read(1), byteorder=order, signed=False)
                detectors.append(detector)


            # Read the end number
            data = ser.read(2)
            int_value = int.from_bytes(data, byteorder=order, signed=False)
            
            # If end number is received, print the values
            if int_value == endNum:
                print(f'Data Mode: {data_mode}, Second Counter: {secondCounter}, Milli Counter: {milliCounter}, Detectors: {detectors}')
        else:
            print(f'Received Num: {int_value}')



import serial

# Open serial port
ser = serial.Serial('COM5', 9600)  # Change 'COM5' to your port name, 9600 to your baud rate

startNum = 256
endNum = 300

# Byte order
order = 'little'

# Main loop to start and end reading data
while True:
    # Read 2 bytes for a 16-bit integer
    data = ser.read(2)

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


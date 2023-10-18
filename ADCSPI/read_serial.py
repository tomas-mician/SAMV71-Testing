import serial
import time
from serial.tools import list_ports
def read_serial():
    # List available COM ports
    ports = list(list_ports.comports())
    num_ports = len(ports)

    # Ask the user to input the mode
    mode = int(input('Please enter the mode you want to use (0 for idle, 1 for bin, 2 for event-by-event): '))
    modeChar = mode.to_bytes(1, 'little')

    if mode == 1:
        count = 1+4+2+64+3+2-1
    elif mode == 2:
        count = 1+4+2+2+4+3-1
    elif mode == 0:
        count = 1+4+2+64+3

    for idx, port_info in enumerate(ports):
        print(f"{idx+1}. {port_info.device}")

    # Ask the user to input the COM port number
    port_num = int(input('Please enter the number of the COM port you want to use: ')) - 1
    device = ports[port_num].device

    baud_rate = 115200
    s = serial.Serial(device, baud_rate)

    # Clear input buffer
    s.flushInput()

    message = b'S' + modeChar
    s.write(message)

    dataArray = []
    i = 0

    while True:  # repeat the loop
        data = int.from_bytes(s.read(1), 'little')

        while data != mode:
            data = int.from_bytes(s.read(1), 'little')

        while s.in_waiting < count:
            time.sleep(0.001)

        data = s.read(count)
        current_row = []
        current_row.append(data[0])

        seconds = int.from_bytes(data[3:7], 'little')
        milliseconds = int.from_bytes(data[7:9], 'little')
        current_row.extend([seconds, milliseconds])

        if mode == 2:  # Event-by-event mode
            microseconds = int.from_bytes(data[9:11], 'little')
            current_row.append(microseconds)
            for val in data[11:15]:
                current_row.append(val)
        else:  # Bin mode
            for val in data[11:75]:
                current_row.append(val)

        print(', '.join(map(str, current_row)))
        dataArray.append(current_row)
        s.write(message)

    s.close()

# Call the function if this script is run as the main module
if __name__ == "__main__":
    read_serial()

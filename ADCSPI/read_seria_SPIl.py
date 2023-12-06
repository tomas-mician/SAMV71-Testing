import serial
from serial.tools import list_ports

def read_serial():
    # List available COM ports
    ports = list(list_ports.comports())
    for idx, port_info in enumerate(ports):
        print(f"{idx+1}. {port_info.device}")

    # Ask the user to input the COM port number
    port_num = int(input('Please enter the number of the COM port you want to use: ')) - 1
    device = ports[port_num].device

    baud_rate = 115200
    s = serial.Serial(device, baud_rate, timeout=1)

    # Clear input buffer
    s.flushInput()

    try:
        while True:  # repeat the loop
            line = s.readline().decode().strip()  # Read a line and decode it
            if line:
                print(f"Received: {line}")
                # Parse the data (example: "Temp: 24.00 C, Pressure: 101325.00 Pa, Humidity: 50.00 %")
                try:
                    parts = line.split(',')
                    temperature = float(parts[0].split(':')[1].strip().split(' ')[0])
                    pressure = float(parts[1].split(':')[1].strip().split(' ')[0])
                    humidity = float(parts[2].split(':')[1].strip().split(' ')[0])
                    print(f"Temperature: {temperature} C, Pressure: {pressure} Pa, Humidity: {humidity} %")
                except (IndexError, ValueError):
                    print("Error parsing data")
    except KeyboardInterrupt:
        print("Interrupted by user")
    finally:
        s.close()

if __name__ == "__main__":
    read_serial()

function read_serial()
% List available COM ports
ports = serialportlist;
num_ports = length(ports);
sample_speed = 'K0';

dataPoints = 7;

dataArray = [];
i = 1;
j = 1;
for k = 1:num_ports
    fprintf('%d. %s\n', k, ports(k))
end

% Ask the user to input the COM port number
port_num = input('Please enter the number of the COM port you want to use: ');
device = ports(port_num);


baud_rate = 115200;
s = serialport(device, baud_rate);

% Clear input buffer
flush(s);

message = ['S',sample_speed]; 
% Write 'S' to the serial port
write(s, message, "uint8");  % 'S' is cast to uint8 because write requires data in byte format
% write(s, sample_speed,"uint8");

% At this point, data should be aligned

while true
% Read 1 byte for command
data = read(s,1,"uint8");
dataArray(i,j) = data;
j = j + 1;

% Read 4 bytes for seconds
data = read(s,4,"uint8");
int32data = uint32(data(1)) + bitshift(uint32(data(2)), 8) ...
           + bitshift(uint32(data(3)), 16) + bitshift(uint32(data(4)), 24);
dataArray(i,j) = int32data;
j = j + 1;


% Read 2 bytes for miliseconds
data = read(s,2,"uint8");
int16data = uint16(data(1)) + bitshift(uint16(data(2)), 8);
dataArray(i,j) = int16data;
j = j + 1;

for index = 1:4
    data = read(s,1,"uint8");
    dataArray(i,j) = data;
    j = j + 1;
end

for l = 1:dataPoints
    fprintf('%d, ',dataArray(i,l));
end
fprintf('\n');


i = i+ 1;
j = 1;

write(s, message, "uint8"); 
% write(s, sample_speed,"uint8");
% pause(1)

end


end
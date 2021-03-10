import serial
py_serial = serial.Serial('/dev/ttyUSB1',baudrate=115200,parity=serial.PARITY_NONE,bytesize=serial.EIGHTBITS,stopbits=serial.STOPBITS_ONE)

print("connected to: " + py_serial.portstr)
while True:
    data_b=py_serial.readline()
    print(data_b.decode())
    data_s=data_b.decode()
    if (data_s=='\x00SA0 \r\n'):
        print("sending data")
        py_serial.write(str.encode('V;229;230;26.9;28%;1;1;1;1;V#') )

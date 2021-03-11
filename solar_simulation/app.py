import serial
import curses
py_serial = serial.Serial('/dev/ttyUSB1',baudrate=115200,parity=serial.PARITY_NONE,bytesize=serial.EIGHTBITS,stopbits=serial.STOPBITS_ONE)
screen = curses.initscr()
asci_Art_bulbon=""" 
     :
 '.  _  .'
-=  (~)  =-   
 .'  #  '.
 """

asci_Art_bulbooff=""" 
     
     _  
    (~)     
     # 
 """
my_window = curses.newwin(5, 15, 0, 0)
bulb_window= curses.newwin(50,50,3,5)
bulb_window.addstr(2,2,asci_Art_bulbooff)
bulb_window.refresh()
i=0
print("connected to: " + py_serial.portstr)
while True:
    data_b=py_serial.readline()
    #print(data_b.decode())
    data_s=data_b.decode()
    if (data_s=='\x00SA0 \r\n'):
       my_window.addstr(2,2,"sending data "+str(i))
       py_serial.write(str.encode('V;229;230;26.9;28%;1;1;1;1;V#') )
       my_window.refresh()
       i += 1
    if (data_s =='\x00SB1 \r\n'):
        print("on")
        bulb_window.clear()
        bulb_window.addstr(2,2,asci_Art_bulbon)
        bulb_window.refresh()
    if (data_s == '\x00SC2 \r\n'):
       print("off")
       bulb_window.clear()
       bulb_window.addstr(2,2,asci_Art_bulbooff)
       bulb_window.refresh()

import serial


ser = serial.Serial('/dev/ttyACM0', 9600) # Change ACM number as found from ls /dev/tty/ACM*
while True:
  print ser.readline()
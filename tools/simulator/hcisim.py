import serial
import time

ser = serial.Serial('COM3', baudrate=57600, bytesize=8,timeout=1, parity=serial.PARITY_EVEN, stopbits=serial.STOPBITS_ONE)  # open serial port


while True:
    busscan = "02 17 9C B9 00 05 9C 41 00 03 06 00 02 00 00 01 02 f8 35"
    print(bytearray.fromhex(busscan))
    ser.write(bytearray.fromhex(busscan))
    #time.sleep(2)
    print(ser.read(15))

    status = "02 17 9C B9 00 08 9C 41 00 02 04 3E 03 00 00 EB CC"
    print(bytearray.fromhex(status))
    ser.write(bytearray.fromhex(status))
    #time.sleep(2)
    print(ser.read(21))

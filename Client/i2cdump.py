#!/usr/bin/python

import serial
import time
import sys
import struct
import os

def FlushInput(ser):
	while ser.inWaiting():
		ch = ser.read(1)
		if len(ch) != 1:
			return

if len(sys.argv) != 5:
	print 'Usage: i2cdump.py <serial_port> <slave_address_in_hex> <size> <output_file>'
	exit(1)

print("+++ Connecting to the BUSSide")
for i in range(3):
	try:
	        ser = serial.Serial(sys.argv[1], 500000, timeout=2)
		FlushInput(ser)
		ser.close() # some weird bug
	except Exception, e:
		ser.close()
	try:
		ser = serial.Serial(sys.argv[1], 500000, timeout=15)
		print("+++ Initiating comms");
		ser.write(b'\n\n\n\n\n')
		ser.write(b'\n\n\n\n\n')
		while True:
			ch = ser.read(1)
			if len(ch) != 1:
				raise Exception("Timeout")
			if (ch == b'>'):
				break;
		print("+++ Sending I2C dump command")
		ser.write(b'i\n')
		while True:
			ch = ser.read(1)
			if len(ch) != 1:
				raise Exception("Timeout")
			if ch == b'.':
				break
		print('+++ Dumping I2C to file...')
		count = 0
                address = int(sys.argv[2], 16)
		size = int(sys.argv[3])
                ser.write(struct.pack('B', address))
		ser.write(struct.pack('>i4', size))
		with open(sys.argv[4], 'wb') as f:
			while True:
				data = ser.read(1)
				if len(data) == 1:
					f.write(data)
					f.flush()
					count = count + 1
					if count == size:
						break
				else:
					raise Exception("Timeout")
		print("+++ SUCCESS\n")
		sys.exit(0)
	except Exception, e:
		ser.close()
		print("*** BUSSide timeout. Didn't read all the data.")
		print("*** This probably means your serial port is")
		print("*** dropping data.")

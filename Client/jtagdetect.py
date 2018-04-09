#!/usr/bin/python

import serial
import time
import sys
import struct

def FlushInput(ser):
	while ser.inWaiting():
		ch = ser.read(1)
		if len(ch) != 1:
			return

if len(sys.argv) != 2:
	print 'Usage: jtagdetect.py <serial_port>'
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
		ser = serial.Serial(sys.argv[1], 500000, timeout=120)
		print("+++ Initiating comms");
		ser.write(b'\n\n\n\n\n')
		ser.write(b'\n\n\n\n\n')
		while True:
			ch = ser.read(1)
			if len(ch) != 1:
				raise Exception("Timeout")
			if (ch == b'>'):
				break;
		print("    Need the attached device powered on.")
		print("	Press enter to continue.");
		ch = raw_input();	
		print("+++ Sending jtag detection command")
		ser.write(b'j\n')
		while True:
			ch = ser.read(1)
			if len(ch) != 1:
				raise Exception("Timeout")
			if ch == b'.':
				break
		print("+++ Reading response from the BUSSide")
		while True:
			data = ser.read(1)
			if len(data) != 1:
				raise Exception("Timeout")
			if data == ';':
				break
			sys.stdout.write(data)
		print("+++ SUCCESS\n")
		sys.exit(0)
	except Exception, e:
		ser.close()
		print("*** BUSSide read timeout as it is non responsive.")
		print("*** Try resetting and try again if it persists.")

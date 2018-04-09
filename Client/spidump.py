#!/usr/bin/python

import binascii
import serial
import time
import sys
import struct
import os

BLOCKSIZE=1024

def dumpSPI(ser, size, skip):
        bigdata = ""
        ser.write(b'\n\n')
        while True:
		ch = ser.read(1)
		if len(ch) != 1:
			raise Exception("Timeout")
		if (ch == b'>'):
			break;
	ser.write(b'r\n')
	while True:
		ch = ser.read(1)
		if len(ch) != 1:
			raise Exception("Timeout")
		if ch == b'.':
			break
	count = 0
	ser.write(struct.pack('>i4', size))
	ser.write(struct.pack('>i4', skip))
	while True:
		data = ser.read(1)
		if len(data) == 1:
                        bigdata = bigdata + data
			count = count + 1
			if count == size:
				break
		else:
			return None
	return bigdata

def FlushInput(ser):
	while ser.inWaiting():
		ch = ser.read(1)
		if len(ch) != 1:
			return

if len(sys.argv) != 4:
	print 'Usage: spidump.py <serial_port> <size> <output_file>'
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
		ser = serial.Serial(sys.argv[1], 500000, timeout=2)
		print("+++ Initiating comms");
		ser.write(b'\n\n\n\n\n')
		ser.write(b'\n\n\n\n\n')
	        dumpsize = int(sys.argv[2])
                skip = 0
                print("+++ Dumping SPI")
	        with open(sys.argv[3], 'wb') as f:
                    while dumpsize > 0:
                        if dumpsize < BLOCKSIZE:
                            size = dumpsize
                        else:
                            size = BLOCKSIZE
                        for j in range(5):
                            data = dumpSPI(ser, size, skip)
                            if data is not None:
                                crcraw = ser.read(4)
                                if len(crcraw) == 4:
                                    crc, = struct.unpack(">i4", crcraw)
                                    if crc == binascii.crc32(data):
                                        break
                                    print("+++ crc mismatch")
                            print("+++ Retransmitting a block")
                        if j == 5:
                            raise "Timeout"
                        f.write(data)
                        f.flush()
                        skip = skip + BLOCKSIZE
                        dumpsize = dumpsize - size 
	        print("+++ SUCCESS\n")
		sys.exit(0)
	except Exception, e:
		ser.close()
		print("*** BUSSide timeout. Didn't read all the data.")
		print("*** This probably means your serial port is")
		print("*** dropping data.")

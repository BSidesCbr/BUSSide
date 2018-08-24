#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct
import os

BLOCKSIZE=1024

def dumpSPI(ser, size, skip):
        bs_command = struct.pack('<I', 1)
        bs_command_length = struct.pack('<I', 0)
        saved_sequence_number = bs.get_sequence_number()
        bs.next_sequence_number()
        bs_request_args  = struct.pack('<I', size)
        bs_request_args += struct.pack('<I', skip)
        bs_request_args += struct.pack('<I', 1000000)
        bs_request_args += struct.pack('<I', 0) * 253
        request  = bs_command
        request += bs_command_length
        request += struct.pack('<I', saved_sequence_number)
        request += bs_request_args
        crc = binascii.crc32(request)
        request += struct.pack('<i', crc)
        ser.write(request)
        bs_command = ser.read(4)
        bs_command_length = ser.read(4)
        bs_sequence_number = ser.read(4)
        bigdata = ""
        count = 0
	while True:
	    if count == 1024:
                break
            else:
	        data = ser.read(1)
	        if data is not None and len(data) == 1:
                    bigdata = bigdata + data
                else:
		    return None
		count = count + 1
        bs_checksum = ser.read(4)
        bs_checksum, = struct.unpack('<i', bs_checksum)
        reply  = bs_command
        reply += bs_command_length
        reply += bs_sequence_number
        reply += bigdata
        crc = binascii.crc32(reply)
        if crc != bs_checksum:
            return None
        seq, = struct.unpack('<I', bs_sequence_number)
        if saved_sequence_number != seq:
            return None
        return bigdata[0:size]

def spi_dump_flash(device, dumpsize, outfile):
    print("+++ Connecting to the BUSSide")
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        bs.FlushInput(ser)
	ser.close() # some weird bug
    except Exception, e:
	ser.close()
    try:
	ser = serial.Serial(device, 500000, timeout=2)
	print("+++ Initiating comms");
        bs.FlushInput(ser)
    except Exception, e:
	ser.close()
	print("*** BUSSide connection error")
        return -1
    skip = 0
    print("+++ Dumping SPI")
    with open(outfile, 'wb') as f:
        while dumpsize > 0:
            if dumpsize < BLOCKSIZE:
                size = dumpsize
            else:
                size = BLOCKSIZE
            for j in range(10):
                try:
                    data = dumpSPI(ser, size, skip)
                    if data is not None:
                        break
                except Exception, e:
                    print(e)
                    pass
                if j > 5:
                    print("--- Trying a hard reset of the serial device.")
                    ser.close()
                    time.sleep(1)
	            ser = serial.Serial(device, 500000, timeout=2)
                    bs.FlushInput(ser)
                print("--- Warning. Retransmiting Attempt #%d" % (j+1))
            if data is None:
                print("Timeout")
                raise "Timeout"
            f.write(data)
            f.flush()
            skip = skip + BLOCKSIZE
            dumpsize = dumpsize - size 
	print("+++ SUCCESS\n")
	return 0

def do_spi_read_id(device):
    print("+++ Connecting to the BUSSide")
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        print("+++ Initiating comms");
        bs.FlushInput(ser)
        ser.close() # some weird bug
    except Exception, e:
        ser.close()
        print("*** BUSSide connection error")
        return -1
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        bs.FlushInput(ser)
    except Exception, e:
        ser.close()
        print("*** BUSSide connection error")
        return -1
    print("+++ Sending SPI read ID command")
    bs_command = struct.pack('<I', 17)
    bs_command_length = struct.pack('<I', 0)
    bs_request_args  = struct.pack('<I', 1000000)
    bs_request_args += struct.pack('<I', 0) * 255
    request  = bs_command
    request += bs_command_length
    saved_sequence_number = bs.get_sequence_number()
    bs.next_sequence_number()
    request += struct.pack('<I', saved_sequence_number)
    request += bs_request_args
    crc = binascii.crc32(request)
    request += struct.pack('<i', crc)
    ser.write(request)
    bs_command = ser.read(4)
    bs_reply_length = ser.read(4)
    bs_sequence_number = ser.read(4)
    reply  = bs_command
    reply += bs_reply_length
    reply += bs_sequence_number
    bs_reply_args = list(range(256))
    for i in range(256):
        bs_reply_args[i] = ser.read(4)
        if len(bs_reply_args[i]) != 4:
                ser.close()
                return None
        reply += bs_reply_args[i]
    bs_checksum, = struct.unpack('<i', ser.read(4))
    crc = binascii.crc32(reply)
    if crc != bs_checksum:
        return -1
    seq, = struct.unpack('<I', bs_sequence_number)
    if saved_sequence_number != seq:
        return -2
    v1, = struct.unpack('<I', bs_reply_args[0])
    v2, = struct.unpack('<I', bs_reply_args[1])
    v3, = struct.unpack('<I', bs_reply_args[2])
    print("+++ SPI ID %.2x%.2x%.2x" % (v1, v2, v3))
    print("+++ SUCCESS\n")
    ser.close()
    return 0

def spi_read_id(device):
    for j in range(10):
        try:
            rv = -2
            while rv == -2:
                rv = do_spi_read_id(device)
                if rv == 0:
                    return 0
        except Exception, e:
            print(e)
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
        bs.sync(device)
    print("--- FAILED")
    return -1


def doFlashCommand(device, command):
    if command.find("readID") == 0:
        spi_read_id(device)
    elif command.find("dump ") == 0:
        args = command[5:].split()
        if len(args) != 2:
            return -1
        spi_dump_flash(device, int(args[0]), args[1])

def doCommand(device, command):
    if command.find("flash ") == 0:
        doFlashCommand(device, command[6:])
    elif command.find("send ") == 0:
        pass

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
        if len(bs_command) != 4:
            return None
        bs_command_length = ser.read(4)
        if len(bs_command_length) != 4:
            return None
        bs_sequence_number = ser.read(4)
        if len(bs_sequence_number) != 4:
            return None
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
    ser = bs.Connect(device)
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
                    print("+++ %s" % (str(e)))
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
	return (1, 1)

def do_spi_read_id(device):
    ser = bs.Connect(device)
    print("+++ Sending SPI read ID command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    rv = bs.requestreply(ser, 17, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    v1 = bs_reply_args[0]
    v2 = bs_reply_args[1]
    v3 = bs_reply_args[2]
    print("+++ SPI ID %.2x%.2x%.2x" % (v1, v2, v3))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def spi_read_id(device):
    for j in range(10):
        try:
            rv = do_spi_read_id(device)
            if rv is not None:
                return rv
        except Exception, e:
            print("+++ %s" % (str(e)))
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
        bs.sync(device)
    print("--- FAILED")
    return None


def doFlashCommand(device, command):
    if command.find("readID") == 0:
        spi_read_id(device)
        return 0
    elif command.find("dump ") == 0:
        args = command[5:].split()
        if len(args) != 2:
            return None
        spi_dump_flash(device, int(args[0]), args[1])
        return 0
    else:
        return None

def doCommand(device, command):
    if command.find("flash ") == 0:
        doFlashCommand(device, command[6:])
        return 0
    elif command.find("send ") == 0:
        print("+++ Not implemented")
        return None
    else:
        return None

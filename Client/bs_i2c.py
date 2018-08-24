#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct

BLOCKSIZE=1024

def do_i2c_discover_slaves(device):
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
    print("+++ Sending i2c slave discovery command")
    bs_command = struct.pack('<I', 5)
    bs_command_length = struct.pack('<I', 0)
    bs_request_args = struct.pack('<I', 0) * 256
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
        s = ser.read(4)
        reply += s
        bs_reply_args[i], = struct.unpack('<I', s)
    bs_checksum, = struct.unpack('<i', ser.read(4))
    crc = binascii.crc32(reply)
    if crc != bs_checksum:
        return -1
    seq, = struct.unpack('<I', bs_sequence_number)
    if saved_sequence_number != seq:
        return -2
    nslave_addresses, = struct.unpack('<I', bs_reply_length)
    print("+++ %d I2C slave addresses" % (nslave_addresses))
    for i in range(nslave_addresses):
        print("+++ I2C slave address FOUND at %i" % bs_reply_args[i])
    print("+++ SUCCESS\n")
    ser.close()
    return 0

def i2c_discover_slaves(device):
    for j in range(10):
        try:
            rv = -2
            while rv == -2:
                rv = do_i2c_discover_slaves(device)
                if rv == 0:
                    return 0
        except Exception, e:
            print(e)
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
    print("--- FAILED")
    return -1


def doFlashCommand(device, command):
    if command.find("readID") == 0:
        doSPIReadID(device)
    elif command.find("dump ") == 0:
        args = command[5:].split()
        if len(args) != 3:
            return -1
        i2c_dump_flash(device, int(args[0]), int(args[1]), args[2])

def doCommand(device, command):
    if command.find("flash ") == 0:
        return doFlashCommand(device, command[6:])
    elif command == "discover slaves":
        return i2c_discover_slaves(device)


def dumpI2C(ser, slave, size, skip):
        bs_command = struct.pack('<I', 9)
        bs_command_length = struct.pack('<I', 0)
        saved_sequence_number = bs.get_sequence_number()
        bs.next_sequence_number()
        bs_request_args  = struct.pack('<I', slave)
        bs_request_args += struct.pack('<I', size)
        bs_request_args += struct.pack('<I', skip)
        bs_request_args += struct.pack('<I', 0) * 253
        request  = bs_command
        request += bs_command_length
        request += struct.pack('<I', saved_sequence_number)
        request += bs_request_args
        crc = binascii.crc32(request)
        request += struct.pack('<i', crc)
        ser.write(request)
        bs_command = ser.read(4)
        bs_reply_length = ser.read(4)
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
        reply += bs_reply_length
        reply += bs_sequence_number
        reply += bigdata
        crc = binascii.crc32(reply)
        if crc != bs_checksum:
            return None
        seq, = struct.unpack('<I', bs_sequence_number)
        if saved_sequence_number != seq:
            return None
        return bigdata[0:size]

def i2c_dump_flash(device, slave, dumpsize, outfile):
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
    print("+++ Dumping I2C")
    with open(outfile, 'wb') as f:
        while dumpsize > 0:
            if dumpsize < BLOCKSIZE:
                size = dumpsize
            else:
                size = BLOCKSIZE
            for j in range(10):
                try:
                    data = dumpI2C(ser, slave, size, skip)
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

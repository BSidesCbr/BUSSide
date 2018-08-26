#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct

BLOCKSIZE=1024

def do_i2c_discover_slaves(device, sda, scl):
    ser = bs.Connect(device)
    print("+++ Sending i2c slave discovery command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = sda
    request_args[1] = scl
    rv = bs.requestreply(ser, 5, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv

    nslave_addresses = bs_reply_length
    print("+++ %d I2C slave addresses" % (nslave_addresses))
    for i in range(nslave_addresses):
        print("+++ I2C slave address FOUND at %i" % bs_reply_args[i])
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def i2c_discover_slaves(device, sda, scl):
    for j in range(10):
        try:
            rv = do_i2c_discover_slaves(device, sda, scl)
            if rv is not None:
                return rv
        except Exception, e:
            print("+++ %s" % (str(e)))
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
    print("--- FAILED")
    return None

def do_i2c_discover(device):
    ser = bs.Connect(device, 30)
    print("+++ Sending i2c discover pinout command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    rv = bs.requestreply(ser, 23, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv

    n = bs_reply_length
    print("+++ FOUND %d I2C interfaces" % (n))
    for i in range(n):
        sda = bs_reply_args[i*2 + 0]
        scl = bs_reply_args[i*2 + 1]
        print("+++ I2C interface FOUND")
        print("+++ I2C SDA at GPIO %i" % (sda))
        print("+++ I2C SCL at GPIO %i" % (scl))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)



def i2c_discover(device):
    for j in range(10):
        try:
            rv = do_i2c_discover(device)
            if rv is not None:
                return rv
        except Exception, e:
            print("+++ %s" % (str(e)))
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
    print("--- FAILED")
    return None

def doFlashCommand(device, command):
    if command.find("dump ") == 0:
        args = command[5:].split()
        if len(args) != 5:
            return None
        i2c_dump_flash(device, int(args[0]), int(args[1]), int(args[2]), int(args[3]), args[4])
        return 0
    else:
        return None

def doCommand(device, command):
    if command.find("flash ") == 0:
        doFlashCommand(device, command[6:])
        return 0
    elif command == "discover pinout":
        i2c_discover(device)
        return 0
    elif command.find("discover slaves ") == 0:
        args = command[16:].split()
        if len(args) != 2:
            return None
        i2c_discover_slaves(device, int(args[0]), int(args[1]))
        return 0
    else:
        return None

def dumpI2C(ser, sda, scl, slave, size, skip):
        bs_command = struct.pack('<I', 9)
        bs_command_length = struct.pack('<I', 0)
        saved_sequence_number = bs.get_sequence_number()
        bs.next_sequence_number()
        bs_request_args  = struct.pack('<I', slave)
        bs_request_args += struct.pack('<I', size)
        bs_request_args += struct.pack('<I', skip)
        bs_request_args += struct.pack('<I', sda)
        bs_request_args += struct.pack('<I', scl)
        bs_request_args += struct.pack('<I', 0) * 251
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
        bs_reply_length = ser.read(4)
        if len(bs_reply_length) != 4:
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
        if len(bs_checksum) != 4:
            return None
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

def i2c_dump_flash(device, sda, scl, slave, dumpsize, outfile):
    ser = bs.Connect(device)
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
                    data = dumpI2C(ser, sda, scl, slave, size, skip)
                    if data is not None:
                        break
                except Exception, e:
                    print("+++ %s" % (str(e)))
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

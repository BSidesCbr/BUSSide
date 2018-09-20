#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct
import os

BLOCKSIZE=1024
WRITEBLOCKSIZE=256

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

def writeSPI(ser, size, skipsize, data):
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = size
    request_args[1] = skipsize
    request_args[2] = 1000000
    for i in range(size / 4):
        request_args[3 + i] = data[i]
    rv = bs.requestreply(ser, 37, 0, request_args)
    return rv

def spi_flash(device, dumpsize, infile):
    ser = bs.Connect(device, 5)
    skip = 0
    print("+++ Writing SPI")
    with open(infile, 'rb') as f:
        while dumpsize > 0:
            if dumpsize < WRITEBLOCKSIZE:
                size = dumpsize
            else:
                size = WRITEBLOCKSIZE
            f.seek(skip)
            rawdata = f.read(size)
            data = list(range(size/4))
            for i in range(size/4):
                a = ord(rawdata[4*i + 0])
                b = ord(rawdata[4*i + 1])
                c = ord(rawdata[4*i + 2])
                d = ord(rawdata[4*i + 3])
                data[i] = (d << 24) + (c << 16) + (b << 8) + a
            for j in range(10):
                try:
                    rv = writeSPI(ser, size, skip, data)
                    if rv is not None:
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
            skip = skip + WRITEBLOCKSIZE
            dumpsize = dumpsize - size
        print("+++ SUCCESS\n")
        return (1, 1)

def spi_fuzz(device, cs, clk, mosi, miso):
    ser = bs.Connect(device, 60)
    print("+++ Sending spi fuzz command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    rv = bs.requestreply(ser, 35, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv

    n = bs_reply_length
    print("+++ FOUND %d SPI commands" % (n))
    for i in range(n):
        cmd = bs_reply_args[i*6 + 0]
        v1 = bs_reply_args[i*6 + 1]
        v2 = bs_reply_args[i*6 + 2]
        v3 = bs_reply_args[i*6 + 3]
        v4 = bs_reply_args[i*6 + 4]
        v5 = bs_reply_args[i*6 + 5]
        print("+++ SPI command FOUND")
        print("+++ SPI command %.2x" % (cmd))
        print("+++ SPI v1 %.2x" % (v1))
        print("+++ SPI v2 %.2x" % (v2))
        print("+++ SPI v3 %.2x" % (v3))
        print("+++ SPI v4 %.2x" % (v4))
        print("+++ SPI v5 %.2x" % (v5))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)


def spi_discover_pinout(device):
    ser = bs.Connect(device, 60)
    print("+++ Sending spi discover pinout command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    rv = bs.requestreply(ser, 29, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv

    n = bs_reply_length
    print("+++ FOUND %d SPI interfaces" % (n))
    for i in range(n):
        cs = bs_reply_args[i*4 + 0]
        clk = bs_reply_args[i*4 + 1]
        mosi = bs_reply_args[i*4 + 2]
        miso = bs_reply_args[i*4 + 3]
        print("+++ SPI interface FOUND")
        print("+++ SPI CS at GPIO %i" % (cs))
        print("+++ SPI CLK at GPIO %i" % (clk))
        print("+++ SPI MOSI at GPIO %i" % (mosi))
        print("+++ SPI MISO at GPIO %i" % (miso))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def spi_streg1(device, cs, clk, mosi, miso):
    ser = bs.Connect(device)
    print("+++ Sending SPI command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    args = [0x05, 0x00]
    n = len(args)
    request_args[5] = n
    for i in range(n):
        request_args[6 + i] = args[i]
    rv = bs.requestreply(ser, 3, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(1, n):
       print("+++ STATUS REGISTER 1: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def spi_streg2(device, cs, clk, mosi, miso):
    ser = bs.Connect(device)
    print("+++ Sending SPI command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    args = [0x35, 0x00]
    n = len(args)
    request_args[5] = n
    for i in range(n):
        request_args[6 + i] = args[i]
    rv = bs.requestreply(ser, 3, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(1, n):
       print("+++ STATUS REGISTER 2: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def spi_readuid(device, cs, clk, mosi, miso):
    ser = bs.Connect(device)
    print("+++ Sending SPI command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    args = [0x4b, 0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0]
    n = len(args)
    request_args[5] = n
    for i in range(n):
        request_args[6 + i] = args[i]
    rv = bs.requestreply(ser, 3, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(5, n):
       print("+++ UID: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def doSendCommand(device, cs, clk, mosi, miso, args):
    ser = bs.Connect(device)
    print("+++ Sending SPI command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    n = len(args)
    request_args[5] = n
    for i in range(n):
        request_args[6 + i] = int(args[i], 16)
    rv = bs.requestreply(ser, 3, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(n):
       print("+++ SPI Response: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def spi_wp_enable(device, cs, clk, mosi, miso):
    ser = bs.Connect(device)
    print("+++ Sending SPI write protect commands")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    rv = bs.requestreply(ser, 41, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def spi_wp_disable(device, cs, clk, mosi, miso):
    ser = bs.Connect(device)
    print("+++ Sending SPI write protect commands")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    rv = bs.requestreply(ser, 39, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def spi_bb_read_id(device, cs, clk, mosi, miso):
    ser = bs.Connect(device)
    print("+++ Sending SPI read ID command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    rv = bs.requestreply(ser, 31, 0, request_args)
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

def spi_erase_sector(device, skipsize, cs, clk, mosi, miso):
    ser = bs.Connect(device)
    print("+++ Sending SPI erase sector command")
    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    request_args[0] = 1000000
    request_args[1] = skipsize
    # the following is ignored for now
    request_args[2] = cs
    request_args[3] = clk
    request_args[4] = mosi
    request_args[5] = miso
    rv = bs.requestreply(ser, 27, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
    print("+++ SUCCESS\n")
    ser.close()
    return (bs_reply_length, bs_reply_args)

def doFlashCommand(device, command):
    if command.find("read id") == 0:
        args = command[7:].split()
        if len(args) == 0:
            spi_read_id(device)
        elif len(args) == 4:
            spi_bb_read_id(device, int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    elif command.find("read sreg1") == 0:
        args = command[10:].split()
        if len(args) == 0:
            spi_streg1(device, 9, 6, 8, 7)
            return 0
        elif len(args) == 4:
            spi_streg1(device, int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    elif command.find("read sreg2") == 0:
        args = command[10:].split()
        if len(args) == 0:
            spi_streg2(device, 9, 6, 8, 7)
            return 0
        elif len(args) == 4:
            spi_streg2(device, int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    elif command.find("read uid") == 0:
        args = command[8:].split()
        if len(args) == 0:
            spi_readuid(device, 9, 6, 8, 7)
        elif len(args) == 4:
            spi_readuid(device, int(args[0]), int(args[1]), int(args[2]), int(args[3]))
        return 0
    elif command == "wp enable":
        spi_wp_enable(device, 9, 6, 8, 7)
        return 0
    elif command == "wp disable":
        spi_wp_disable(device, 9, 6, 8, 7)
        return 0
    elif command.find("write ") == 0:
        args = command[5:].split()
        if len(args) != 2:
            return None
        spi_flash(device, int(args[0]), args[1])
        return 0
    elif command.find("dump ") == 0:
        args = command[5:].split()
        if len(args) != 2:
            return None
        spi_dump_flash(device, int(args[0]), args[1])
        return 0
    elif command.find("erase sector ") == 0:
        args = command[12:].split()
        if len(args) == 1:
            spi_erase_sector(device, int(args[0]), 9, 6, 8, 7)
            return 0
        else:
            return None
    else:
        return None

def doCommand(device, command):
    if command.find("flash ") == 0:
        doFlashCommand(device, command[6:])
        return 0
    elif command.find("send default ") == 0:
        args = command[12:].split()
        if args == 5:
            return None
        doSendCommand(device, 9, 6, 8, 7, args)
        return 0
    elif command.find("send ") == 0:
        args = command[4:].split()
        if args < 5:
            return None
        doSendCommand(device, int(args[0]), int(args[1]), int(args[2]), int(args[3]), args[4:])
        return 0
    elif command == "discover pinout":
        spi_discover_pinout(device)
        return 0
    elif command.find("fuzz ") == 0:
        args = command[4:].split()
        if len(args) == 4:
            spi_fuzz(device, int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    else:
        return None

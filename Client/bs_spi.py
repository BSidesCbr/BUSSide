#!/usr/bin/python

import bs
import binascii
import time
import sys
import struct
import os

BLOCKSIZE=1024
WRITEBLOCKSIZE=256

def dumpSPI(size, skip):
    request_args = [size, skip, 1000000]
    rv = bs.requestreply(1, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    data = ""
    for i in range(bs_reply_length / 4):
        data = data + struct.pack('<I', bs_reply_args[i])
    return data

def spi_dump_flash(dumpsize, outfile):
    bs.NewTimeout(5)
    skip = 0
    print("+++ Dumping SPI")
    with open(outfile, 'wb') as f:
        while dumpsize > 0:
            if dumpsize < BLOCKSIZE:
                size = dumpsize
            else:
                size = BLOCKSIZE
            data = dumpSPI(size, skip)
            if data is None:
                print("Timeout")
                return None
            f.write(data)
            f.flush()
            skip = skip + BLOCKSIZE
            dumpsize = dumpsize - size 
	print("+++ SUCCESS\n")
	return (1, 1)

def spi_read_id():
    print("+++ Sending SPI read ID command")
    request_args = [1000000]
    rv = bs.requestreply(17, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    v1 = bs_reply_args[0]
    v2 = bs_reply_args[1]
    v3 = bs_reply_args[2]
    print("+++ SPI ID %.2x%.2x%.2x" % (v1, v2, v3))
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def writeSPI(size, skipsize, data):
    request_args = list(range(3 + size/4))
    request_args[0] = size
    request_args[1] = skipsize
    request_args[2] = 1000000
    for i in range(size / 4):
        request_args[3 + i] = data[i]
    rv = bs.requestreply(37, request_args)
    return rv

def spi_flash(dumpsize, infile):
    bs.NewTimeout(5)
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
            rv = writeSPI(size, skip, data)
            if rv is None:
                print("Timeout")
                return None
            skip = skip + WRITEBLOCKSIZE
            dumpsize = dumpsize - size
        print("+++ SUCCESS\n")
        return (1, 1)

def spi_fuzz(cs, clk, mosi, miso):
    print("+++ Sending spi fuzz command")
    request_args = [1000000, cs, clk, mosi, miso]
    bs.NewTimeout(60)
    rv = bs.requestreply(35, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv

    n = bs_reply_length / (4*6)
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
    return (bs_reply_length, bs_reply_args)


def spi_discover_pinout():
    print("+++ Sending spi discover pinout command")
    request_args = [1000000]
    bs.NewTimeout(60)
    rv = bs.requestreply(29, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv

    n = bs_reply_length / (4*4)
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
    return (bs_reply_length, bs_reply_args)

def spi_streg1(cs, clk, mosi, miso):
    print("+++ Sending SPI command")
    request_args = [1000000, cs, clk, mosi, miso, 2, 0x05, 0x00]
    rv = bs.requestreply(3, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(1, 2):
       print("+++ STATUS REGISTER 1: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def spi_streg2(cs, clk, mosi, miso):
    print("+++ Sending SPI command")
    request_args = [1000000, cs, clk, mosi, miso, 2, 0x35, 0x00]
    rv = bs.requestreply(3, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(1, 2):
       print("+++ STATUS REGISTER 2: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def spi_readuid(cs, clk, mosi, miso):
    print("+++ Sending SPI command")
    request_args = [1000000, cs, clk, mosi, miso, 13, 0x4b, 0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0]
    rv = bs.requestreply(3, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(5, 13):
       print("+++ UID: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def doSendCommand(cs, clk, mosi, miso, args):
    print("+++ Sending SPI command")
    n = len(args)
    request_args = list(range(6 + n))
    request_args[0] = 1000000
    request_args[1] = cs
    request_args[2] = clk
    request_args[3] = mosi
    request_args[4] = miso
    request_args[5] = n
    for i in range(n):
        request_args[6 + i] = int(args[i], 16)
    rv = bs.requestreply(3, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    for i in range(n):
       print("+++ SPI Response: %.2x" % (bs_reply_args[i]))
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def spi_wp_enable(cs, clk, mosi, miso):
    print("+++ Sending SPI write protect commands")
    request_args = [1000000, cs, clk, mosi, miso]
    rv = bs.requestreply(41, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def spi_wp_disable(cs, clk, mosi, miso):
    print("+++ Sending SPI write protect commands")
    request_args = [1000000, cs, clk, mosi, miso]
    rv = bs.requestreply(39, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def spi_bb_read_id(cs, clk, mosi, miso):
    print("+++ Sending SPI read ID command")
    request_args = [1000000, cs, clk, mosi, miso]
    rv = bs.requestreply(31, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    v1 = bs_reply_args[0]
    v2 = bs_reply_args[1]
    v3 = bs_reply_args[2]
    print("+++ SPI ID %.2x%.2x%.2x" % (v1, v2, v3))
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def spi_erase_sector(skipsize, cs, clk, mosi, miso):
    print("+++ Sending SPI erase sector command")
    request_args = [1000000, skipsize, cs, clk, mosi, miso]
    rv = bs.requestreply(27, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    print("+++ SUCCESS\n")
    return (bs_reply_length, bs_reply_args)

def doFlashCommand(command):
    if command.find("read id") == 0:
        args = command[7:].split()
        if len(args) == 0:
            spi_read_id()
        elif len(args) == 4:
            spi_bb_read_id(int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    elif command.find("read sreg1") == 0:
        args = command[10:].split()
        if len(args) == 0:
            spi_streg1(9, 6, 8, 7)
            return 0
        elif len(args) == 4:
            spi_streg1(int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    elif command.find("read sreg2") == 0:
        args = command[10:].split()
        if len(args) == 0:
            spi_streg2(9, 6, 8, 7)
            return 0
        elif len(args) == 4:
            spi_streg2(int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    elif command.find("read uid") == 0:
        args = command[8:].split()
        if len(args) == 0:
            spi_readuid(9, 6, 8, 7)
        elif len(args) == 4:
            spi_readuid(int(args[0]), int(args[1]), int(args[2]), int(args[3]))
        return 0
    elif command == "wp enable":
        spi_wp_enable(9, 6, 8, 7)
        return 0
    elif command == "wp disable":
        spi_wp_disable(9, 6, 8, 7)
        return 0
    elif command.find("write ") == 0:
        args = command[5:].split()
        if len(args) != 2:
            return None
        spi_flash(int(args[0]), args[1])
        return 0
    elif command.find("dump ") == 0:
        args = command[5:].split()
        if len(args) != 2:
            return None
        spi_dump_flash(int(args[0]), args[1])
        return 0
    elif command.find("erase sector ") == 0:
        args = command[12:].split()
        if len(args) == 1:
            spi_erase_sector(int(args[0]), 9, 6, 8, 7)
            return 0
        else:
            return None
    else:
        return None

def doCommand(command):
    if command.find("flash ") == 0:
        doFlashCommand(command[6:])
        return 0
    elif command.find("send default ") == 0:
        args = command[12:].split()
        if args == 5:
            return None
        doSendCommand(9, 6, 8, 7, args)
        return 0
    elif command.find("send ") == 0:
        args = command[4:].split()
        if args < 5:
            return None
        doSendCommand(int(args[0]), int(args[1]), int(args[2]), int(args[3]), args[4:])
        return 0
    elif command == "discover pinout":
        spi_discover_pinout()
        return 0
    elif command.find("fuzz ") == 0:
        args = command[4:].split()
        if len(args) == 4:
            spi_fuzz(int(args[0]), int(args[1]), int(args[2]), int(args[3]))
            return 0
        else:
            return None
    else:
        return None

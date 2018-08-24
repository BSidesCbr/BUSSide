#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct

def do_jtag_discover_pinout(device):
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
        ser = serial.Serial(device, 500000, timeout=30)
        bs.FlushInput(ser)
    except Exception, e:
        ser.close()
        print("*** BUSSide connection error")
        return -1
    print("+++ Sending jtag pinout discovery command")
    bs_command = struct.pack('<I', 13)
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
    if bs_reply_args[0] != 0:
        tck = bs_reply_args[1]
        tms = bs_reply_args[2]
        tdi = bs_reply_args[3]
        tdo = bs_reply_args[4]
        ntrst = bs_reply_args[5]
        print("+++ %d JTAG FOUND" % (bs_reply_args[0]))
        print("+++ TCK %i" % (tck))
        print("+++ TMS %i" % (tms))
        print("+++ TDI %i" % (tdi))
        print("+++ TDO %i" % (tdo))
        print("+++ NTRST %i" % (ntrst))
    print("+++ SUCCESS")
    ser.close()
    return 0

def jtag_discover_pinout(device):
    for j in range(10):
        try:
            rv = -2
            while rv == -2:
                rv = do_jtag_discover_pinout(device)
                if rv == 0:
                    return 0
        except Exception, e:
            print(e)
        print("--- Error. Retransmiting Attempt #%d" % (j+1))
    print("--- FAILED")
    return -1

def doCommand(device, command):
    if command == "discover pinout":
        return jtag_discover_pinout(device)

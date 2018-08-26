#!/usr/bin/python

import time
import struct
import os
import sys
import serial
import binascii
import select
import tty
import termios
import fcntl

sequence_number = 5
oldterm = 0
oldflags = 0

def keys_isData():
    return select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], [])

def keys_init():
    global oldterm
    global oldflags

    fd = sys.stdin.fileno()
    newattr = termios.tcgetattr(fd)
    newattr[3] = newattr[3] & ~termios.ICANON
    newattr[3] = newattr[3] & ~termios.ECHO
    termios.tcsetattr(fd, termios.TCSANOW, newattr)

    oldterm = termios.tcgetattr(fd)
    oldflags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, oldflags | os.O_NONBLOCK)

def keys_getchar():
    if keys_isData():
        return sys.stdin.read(1)
    else:
        return None

def keys_cleanup():
    global oldterm
    global oldflags

    termios.tcsetattr(fd, termios.TCSAFLUSH, oldterm)
    fcntl.fcntl(fd, fcntl.F_SETFL, oldflags)

def get_sequence_number():
    global sequence_number

    return sequence_number

def next_sequence_number():
    global sequence_number

    sequence_number = (sequence_number + 1 ) % (1 << 24)

def FlushInput(ser):
    while ser.inWaiting():
        ch = ser.read(1)
        if len(ch) != 1:
            return

def requestreply(ser, command, command_length, request_args):
    bs_command = struct.pack('<I', command)
    bs_command_length = struct.pack('<I', command_length)
    bs_request_args = ""
    for i in range(256):
        bs_request_args += struct.pack('<I', request_args[i])
    request  = bs_command
    request += bs_command_length
    saved_sequence_number = get_sequence_number()
    next_sequence_number()
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
    reply  = bs_command
    reply += bs_reply_length
    reply += bs_sequence_number
    bs_reply_length, = struct.unpack('<I', bs_reply_length)
    bs_reply_args = list(range(256))
    for i in range(256):
        s = ser.read(4)
        if len(s) != 4:
            return None
        reply += s
        bs_reply_args[i], = struct.unpack('<I', s)
    d = ser.read(4)
    if len(d) != 4:
        return None
    bs_checksum, = struct.unpack('<i', d)
    crc = binascii.crc32(reply)
    if crc != bs_checksum:
        return None
    seq, = struct.unpack('<I', bs_sequence_number)
    if saved_sequence_number != seq:
        return None
    return (bs_reply_length, bs_reply_args)

def Connect(device, mytimeout=2):
    print("+++ Connecting to the BUSSide")
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        print("+++ Initiating comms");
        FlushInput(ser)
        ser.close() # some weird bug
    except Exception, e:
        print("+++ ", e)
        ser.close()
        print("*** BUSSide connection error")
        return None
    try:
        ser = serial.Serial(device, 500000, timeout=mytimeout)
        FlushInput(ser)
    except Exception, e:
        print("+++ %s" % (str(e)))
        ser.close()
        print("*** BUSSide connection error")
        return None
    return ser

def do_sync(device):
    ser = Connect(device)
    print("+++ Sending echo command")
    try:
        request_args = list(range(256))
        for i in range(256):
            request_args[i] = 0
        rv = requestreply(ser, 0, 0, request_args)
        if rv is None:
            ser.close()
            return None
        (bs_reply_length, bs_reply_args) = rv
        print("+++ OK")
        return rv
    except Exception, e:
        print("+++ %s" % (str(e)))
        ser.close()
        return None

def sync(device):
    for j in range(10):
        try:
            rv = do_sync(device)
            if rv is not None:
                return rv 
        except Exception, e:
            print("+++ %s" % (str(e)))
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
        time.sleep(2)
    return None


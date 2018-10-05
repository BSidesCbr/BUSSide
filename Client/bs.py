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

mydevice = None
mytimeout = 2
myserial = None
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

def set_sequence_number(seq):
    global sequence_number

    sequence_number = seq

def get_sequence_number():
    global sequence_number

    return sequence_number

def next_sequence_number():
    global sequence_number

    sequence_number = (sequence_number + 1 ) % (1 << 30)
    with open("/tmp/BUSSide.seq", "wb") as f:
        f.write(struct.pack('<I', sequence_number))

def FlushInput():
    global myserial

    myserial.flushInput()

def Sync():
    global myserial

    ch1 = myserial.read(1)
    ch2 = myserial.read(1)
    if len(ch1) != 1 or len(ch2) != 1:
        return False
    return ord(ch1) == 0xfe and ord(ch2) == 0xca

def requestreply(command, request_args, nretries=10):
    global myserial
    global mydevice
    global mytimeout

    if myserial is None:
        rv = Connect()
        if rv is None:
            return None

    for i in range(nretries):
        if i > 0:
            print("+++ Retransmitting %d/10" % (i))
            if i > 3:
                FlushInput()
                time.sleep(5)
                rv = Connect(mydevice, mytimeout, 0)
                if rv is None:
                    continue

        # build beginning
        bs_sync = "\xfe\xca"
        bs_command = struct.pack('<I', command)
        bs_command_length = struct.pack('<I', len(request_args) * 4)
        bs_request_args = ""
        for i in range(len(request_args)):
            bs_request_args += struct.pack('<I', request_args[i])

        # calculate crc
        request  = bs_command
        request += bs_command_length
        saved_sequence_number = get_sequence_number()
        next_sequence_number()
        request += struct.pack('<I', saved_sequence_number)
        request += struct.pack('<I', 0x00000000)
        request += bs_request_args
        crc = binascii.crc32(request)
        
        # build frame
        request  = bs_command
        request += bs_command_length
        request += struct.pack('<I', saved_sequence_number)
        request += struct.pack('<i', crc)
        request += bs_request_args

        myserial.write(bs_sync + request)
        myserial.flush()

        if not Sync():
            continue

        # read reply header
        bs_command = myserial.read(4)
        if len(bs_command) != 4:
            continue
        bs_reply_length = myserial.read(4)
        if len(bs_reply_length) != 4:
            continue
        reply_length, = struct.unpack('<I', bs_reply_length)
        if reply_length > 65356:
            continue
        bs_sequence_number = myserial.read(4)
        if len(bs_sequence_number) != 4:
            continue
        seq, = struct.unpack('<I', bs_sequence_number)
        d = myserial.read(4)
        if len(d) != 4:
            continue
        bs_checksum, = struct.unpack('<i', d)
     
        # read reply payload
        reply_args = ""
        if reply_length == 0:
            bs_reply_args = []
        else:
            bs_reply_args = list(range(reply_length / 4))
            fail = False
            for i in range(reply_length / 4):
                s = myserial.read(4)
                if len(s) != 4:
                    fail = True
                    break
                reply_args += s
                bs_reply_args[i], = struct.unpack('<I', s)
            if fail:
                continue

        # calculate checksum
        reply  = bs_command
        reply += bs_reply_length
        reply += bs_sequence_number
        reply += struct.pack('<I', 0x00000000)
        reply += reply_args
        crc = binascii.crc32(reply)

        # error checks
        if crc != bs_checksum:
            continue
        if saved_sequence_number != seq:
            continue

        # frame ok
        return (reply_length, bs_reply_args)

    # retries failed
    return None

def getSerial():
    global myserial

    return myserial

def NewTimeout(ltimeout):
    global mydevice

    Connect(mydevice, ltimeout, 0)

def Connect(device, ltimeout=2, nretries=10):
    global myserial
    global mytimeout
    global mydevice

    print("+++ Connecting to the BUSSIde")

    if myserial is not None:
        myserial.close()
        myserial = None

    mydevice = device
    mytimeout = ltimeout
    if nretries == 0:
        n = 1
    else:
        n = nretries
    for i in range(n):
        try:
            myserial = serial.Serial(mydevice, 500000, timeout=mytimeout)
            FlushInput()
            request_args = []
            if nretries > 0:
                print("+++ Sending echo command")
                rv = requestreply(0, request_args, 1)
                if rv is None:
                    myserial.close()
                    continue
                (bs_reply_length, bs_reply_args) = rv
                print("+++ OK")
                return rv
            return (1,1)
        except:
            pass
    return None

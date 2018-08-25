#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct

def do_uart_tx(device, rxpin, baudrate):
    print("+++ Connecting to the BUSSide")
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        print("+++ Initiating comms");
        bs.FlushInput(ser)
        ser.close() # some weird bug
    except Exception, e:
        print(e)
        ser.close()
        print("*** BUSSide connection error")
        return -1
    try:
        ser = serial.Serial(device, 500000, timeout=10)
        bs.FlushInput(ser)
    except Exception, e:
        print(e)
        ser.close()
        print("*** BUSSide connection error")
        return -1
    print("+++ Sending UART discovery tx command")
    try:
        bs_command = struct.pack('<I', 21)
        bs_command_length = struct.pack('<I', 0)
        bs_request_args  = struct.pack('<I', rxpin - 1)
        bs_request_args += struct.pack('<I', baudrate)
        bs_request_args += struct.pack('<I', 0) * 254
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
            if len(s) != 4:
                return -1
            reply += s
            bs_reply_args[i], = struct.unpack('<I', s)
        bs_checksum, = struct.unpack('<i', ser.read(4))
        crc = binascii.crc32(reply)
        if crc != bs_checksum:
            return -1
        seq, = struct.unpack('<I', bs_sequence_number)
        if saved_sequence_number != seq:
            return -2
        txpin = bs_reply_args[0]
        if txpin != 0xffffffff:
            print("+++ FOUND UART TX on GPIO %d" % (txpin + 1))
        else:
            print("+++ NOT FOUND. Note that GPIO 1 can't be used here.")
        ser.close()
        print("+++ SUCCESS")
        return 0
    except Exception, e:
        print(e)
        ser.close()
        return -1

def uart_tx(device, rxpin, baudrate):
    for j in range(10):
        try:
            rv = -2
            while rv == -2:
                rv = do_uart_tx(device, rxpin, baudrate)
                if rv == 0:
                    return 0
        except Exception, e:
            print(e)
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
        time.sleep(2)
    return -1


def do_uart_rx(device):
    print("+++ Connecting to the BUSSide")
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        print("+++ Initiating comms");
        bs.FlushInput(ser)
        ser.close() # some weird bug
    except Exception, e:
        print(e)
        ser.close()
        print("*** BUSSide connection error")
        return -1
    try:
        ser = serial.Serial(device, 500000, timeout=10)
        bs.FlushInput(ser)
    except Exception, e:
        print(e)
        ser.close()
        print("*** BUSSide connection error")
        return -1
    print("+++ Sending UART discovery command")
    try:
        bs_command = struct.pack('<I', 11)
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
        ngpio, = struct.unpack('<I', bs_reply_length)
        reply += bs_sequence_number
        bs_reply_args = list(range(256))
        for i in range(256):
            s = ser.read(4)
            if len(s) != 4:
                return -1
            reply += s
            bs_reply_args[i], = struct.unpack('<I', s)
        bs_checksum, = struct.unpack('<i', ser.read(4))
        crc = binascii.crc32(reply)
        if crc != bs_checksum:
            return -1
        seq, = struct.unpack('<I', bs_sequence_number)
        if saved_sequence_number != seq:
            return -2
        for i in range(ngpio):
            changes = bs_reply_args[i]
            print("+++ GPIO %d has %d signal changes" % (i+1, changes))
            if changes > 0:
                databits = bs_reply_args[ngpio + 4*i + 0]
                if databits > 0:
                    stopbits = bs_reply_args[ngpio + 4*i + 1]
                    parity = bs_reply_args[ngpio + 4*i + 2]
                    baudrate = bs_reply_args[ngpio + 4*i + 3]
                    print("+++ UART FOUND")
                    print("+++ DATABITS: %d" % (databits))
                    print("+++ STOPBITS: %d" % (stopbits))
                    if parity == 0:
                        print("+++ PARITY: EVEN")
                    elif parity == 1:
                        print("+++ PARITY: ODD")
                    else:
                        print("+++ PARITY: NONE")
                    print("+++ BAUDRATE: %d" % (baudrate))
        ser.close()
        return 0
    except Exception, e:
        print(e)
        ser.close()
        return -1

def uart_rx(device):
    for j in range(10):
        try:
            rv = -2
            while rv == -2:
                rv = do_uart_rx(device)
                if rv == 0:
                    return 0
        except Exception, e:
            print(e)
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
        time.sleep(2)
    return -1


def do_uart_passthrough(device, gpiorx, gpiotx, baudrate):
    print("+++ Connecting to the BUSSide")
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        print("+++ Initiating comms");
        bs.FlushInput(ser)
        ser.close() # some weird bug
    except Exception, e:
        print(e)
        ser.close()
        print("*** BUSSide connection error")
        return -1
    try:
        ser = serial.Serial(device, 500000, timeout=2)
        bs.FlushInput(ser)
    except Exception, e:
        print(e)
        ser.close()
        print("*** BUSSide connection error")
        return -1
    print("+++ Sending UART passthrough command")
    try:
        bs_command = struct.pack('<I', 19)
        bs_command_length = struct.pack('<I', 0)
        bs_request_args  = struct.pack('<i', gpiorx - 1)
        bs_request_args += struct.pack('<i', gpiotx - 1)
        bs_request_args += struct.pack('<I', baudrate)
        bs_request_args += struct.pack('<I', 0) * 253
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
            if len(s) != 4:
                return 0
            reply += s
        bs_reply_args[i], = struct.unpack('<I', s)
        bs_checksum, = struct.unpack('<i', ser.read(4))
        crc = binascii.crc32(reply)
        if crc != bs_checksum:
            return -2
        seq, = struct.unpack('<I', bs_sequence_number)
        if saved_sequence_number != seq:
            return -1
        print("+++ Entering passthrough mode")
        bs.keys_init()
        while True:
            if ser.inWaiting() > 0:
                ch = ser.read(1)
                sys.stdout.write(ch)
                sys.stdout.flush()
            inCh = bs.keys_getchar()
            if inCh is not None:
                ser.write(inCh)
        bs.keys_cleanup()
        ser.write(0x69)
        ser.close()
        return 0
    except Exception, e:
        print(e)
        ser.close()
        return -1

def uart_passthrough(device, rxpin, txpin, baudrate):
    for j in range(10):
        try:
            rv = -2
            while rv == -2:
                rv = do_uart_passthrough(device, rxpin, txpin, baudrate)
                if rv == 0:
                    return 0
        except Exception, e:
            print(e)
        print("--- Warning. Retransmiting Attempt #%d" % (j+1))
    print("--- FAILED")
    return -1




def doCommand(device, command):
    if command == "discover rx":
        uart_rx(device)
    elif command.find("discover tx ") == 0:
        args = command[12:].split()
        if len(args) != 2:
            return -1
        uart_tx(device, int(args[0]), int(args[1]))
    elif command.find("passthrough ") == 0:
        args = command[12:].split()
        if len(args) != 3:
            return -1
        uart_passthrough(device, int(args[0]), int(args[1]), int(args[2]))


#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct

def uart_data_discover():
    print("+++ Sending UART data discovery command")
    request_args = []
    bs.NewTimeout(60)
    rv = bs.requestreply(15, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv

    ngpio = 9
    for i in range(ngpio):
        print("+++ SIGNAL CHANGES: D%d --> %d" % ((i+1), bs_reply_args[i]))
    print("+++ SUCCESS")
    return rv


def uart_tx(rxpin, baudrate):
    print("+++ Sending UART discovery tx command")
    request_args = [rxpin, baudrate]
    bs.NewTimeout(3)
    rv = bs.requestreply(21, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv

    txpin = bs_reply_args[0]
    if txpin != 0xffffffff:
        print("+++ FOUND UART TX on GPIO %d" % (txpin + 1))
    else:
        print("+++ NOT FOUND. Note that GPIO 1 can't be used here.")
    print("+++ SUCCESS")
    return rv

def uart_rx():
    print("+++ Sending UART discovery rx command")
    request_args = []
    bs.NewTimeout(120)
    rv = bs.requestreply(11, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv

    ngpio = 9
    for i in range(ngpio):
        changes = bs_reply_args[5*i + 0]
        print("+++ GPIO %d has %d signal changes" % (i+1, changes))
        if changes > 0:
            databits = bs_reply_args[5*i + 1]
            if databits > 0:
                stopbits = bs_reply_args[5*i + 2]
                parity = bs_reply_args[5*i + 3]
                baudrate = bs_reply_args[5*i + 4]
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
    print("+++ SUCCESS")
    return (bs_reply_length, bs_reply_args)

def uart_passthrough(gpiorx, gpiotx, baudrate):
    request_args = [gpiorx-1, gpiotx-1, baudrate]
    bs.NewTimeout(30)
    rv = bs.requestreply(19, request_args)
    if rv is None:
        return None
    (bs_reply_length, bs_reply_args) = rv
    print("+++ Entering passthrough mode")
    bs.keys_init()
    ser = bs.getSerial()
    while True:
        if ser.inWaiting() > 0:
            ch = ser.read(1)
            sys.stdout.write(ch)
            sys.stdout.flush()
        inCh = bs.keys_getchar()
        if inCh is not None:
            ser.write(inCh)
        # not reached
    bs.keys_cleanup()
    return None

def uart_passthrough_auto():
    rv = uart_rx()
    if rv is None:
        print("+++ NOT FOUND")
        return 0
    (bs_reply_length, bs_reply_args) = rv
    uartcount = 0
    ngpio = 9
    for i in range(ngpio):
        changes = bs_reply_args[5*i + 0]
        if changes > 0:
            databits = bs_reply_args[5*i + 1]
            if databits > 0:
                stopbits = bs_reply_args[5*i + 2]
                parity = bs_reply_args[5*i + 3]
                baudrate = bs_reply_args[5*i + 4]
                rxpin = i + 1
                uartcount = uartcount + 1
    if uartcount == 0:
        print("+++ NOT FOUND")
        return 0
    if uartcount > 1:
        print("+++ More than 1 UART device found.")
        print("+++ You will need to do tx discovery and passthrough manually.")
        return None
    print("+++ Sleeping for 60 seconds to get an idle UART.")
    time.sleep(60)
    for j in range(5):
        rv = uart_tx(rxpin, baudrate)
        if rv is not None:
            (bs_reply_length, bs_reply_args) = rv
            txpin = bs_reply_args[0]
            if txpin != 0xffffffff:
                txpin = txpin + 1
                break
        print("+++ Didn't detect TX. Sleeping for 10 seconds and trying again.")
        time.sleep(10)
    if txpin == 0xffffffff:
        print("+++ FAILED")
        return None
    uart_passthrough(rxpin, txpin, baudrate)
    return 0

def doCommand(command):
    if command == "discover rx":
        uart_rx()
        return 0
    elif command == "discover data":
        uart_data_discover()
        return 0
    elif command.find("discover tx ") == 0:
        args = command[12:].split()
        if len(args) != 2:
            return None
        uart_tx(int(args[0]), int(args[1]))
        return 0
    elif command == ("passthrough auto"):
        uart_passthrough_auto()
        return 0
    elif command.find("passthrough ") == 0:
        args = command[12:].split()
        if len(args) != 3:
            return None
        uart_passthrough(int(args[0]), int(args[1]), int(args[2]))
        return 0
    else:
        return None


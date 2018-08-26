#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct

def do_jtag_discover_pinout(device):
    ser = bs.Connect(device, 30)
    print("+++ Sending jtag pinout discovery command")

    request_args = list(range(256))
    for i in range(256):
        request_args[i] = 0
    rv = bs.requestreply(ser, 13, 0, request_args)
    if rv is None:
        ser.close()
        return None
    (bs_reply_length, bs_reply_args) = rv
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
    return (bs_reply_length, bs_reply_args)

def jtag_discover_pinout(device):
    for j in range(10):
        try:
            rv = do_jtag_discover_pinout(device)
            if rv is not None:
                return rv
        except Exception, e:
            print("+++ ", e)
        print("--- Error. Retransmiting Attempt #%d" % (j+1))
    print("--- FAILED")
    return None

def doCommand(device, command):
    if command == "discover pinout":
        jtag_discover_pinout(device)
        return 0
    else:
        return None

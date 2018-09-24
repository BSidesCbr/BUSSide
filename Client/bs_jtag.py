#!/usr/bin/python

import bs
import binascii
import serial
import time
import sys
import struct

def jtag_discover_pinout():
    print("+++ Sending jtag pinout discovery command")

    request_args = []
    bs.NewTimeout(30)
    rv = bs.requestreply(13, request_args)
    if rv is None:
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
    return (bs_reply_length, bs_reply_args)

def doCommand(command):
    if command == "discover pinout":
        jtag_discover_pinout()
        return 0
    else:
        return None

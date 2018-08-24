#!/usr/bin/python

import os
import sys
import bs_spi
import bs_i2c
import bs_uart
import bs_jtag
import bs

sequence_number = 5

if len(sys.argv) != 2:
    print("Usage: %s <serdevice>" % (sys.argv[0]))
    sys.exit(0)

device = sys.argv[1]

def printHelp():
    print("spi send <cmd1> ....")
    print("spi flash readID")
    print("spi flash dump <size> <outfile>")
    print("i2c discover slaves")
    print("i2c flash dump <slaveAddress> <size> <outfile>")
    print("jtag discover pinout")
    print("quit")
    print("")

def doCommand(device, command):
    if command.find("spi ") == 0:
        bs_spi.doCommand(device, command[4:])
    elif command.find("i2c ") == 0:
        bs_i2c.doCommand(device, command[4:])
    elif command.find("uart ") == 0:
        bs_uart.doCommand(device, command[5:])
    elif command.find("jtag ") == 0:
        bs_jtag.doCommand(device, command[5:])
    elif command == "quit":
        return -1
    else:
        printHelp()
    return 0

bs.sync(device)

while True:
    command = raw_input("> ")
    if doCommand(device, command) < 0:
        break

print("Bye!")

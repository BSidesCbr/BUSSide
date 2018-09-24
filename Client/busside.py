#!/usr/bin/python

import struct
import os
import sys
import bs_i2c
import bs_uart
import bs_jtag
import bs
import bs_spi

sequence_number = 5

if len(sys.argv) != 2:
    print("Usage: %s <serdevice>" % (sys.argv[0]))
    sys.exit(0)

device = sys.argv[1]

def printHelp():
    print("+++ The BUSSide accepts the following commands")
    print("+++")
    print("+++ > spi discover pinout")
    print("+++ > spi send default <cmdbyte1> ....")
    print("+++ > spi send <cs> <clk> <mosi> <miso> <cmdbyte1> ....")
    print("+++ > spi fuzz <cs> <clk> <mosi> <miso>")
    print("+++ > spi flash erase sector <address>")
    print("+++ > spi flash wp enable|disable")
    print("+++ > spi flash read id [<cs> <clk> <mosi> <miso>]")
    print("+++ > spi flash read uid [<cs> <clk> <mosi> <miso>]")
    print("+++ > spi flash read sreg1 [<cs> <clk> <mosi> <miso>]")
    print("+++ > spi flash read sreg2 [<cs> <clk> <mosi> <miso>]")
    print("+++ > spi flash dump <size> <outfile>")
    print("+++ > spi flash write <size> <infile>")
    print("+++ > i2c discover pinout")
    print("+++ > i2c discover slaves <sdaPin> <sclPin>")
    print("+++ > i2c flash dump <sdaPin> <sclPin> <slaveAddress> <addressLength> <size> <outfile>")
    print("+++ > i2c flash write <sdaPin> <sclPin> <slaveAddress> <addressLength> <size> <infile>")
    print("+++ > jtag discover pinout")
    print("+++ > uart discover data")
    print("+++ > uart passthrough auto")
    print("+++ > uart discover rx")
    print("+++ > uart discover tx <rx_gpio> <baudrate>")
    print("+++ > uart passthrough <rx_gpio> <tx_gpio> <baudrate>")
    print("+++ > help")
    print("+++ > quit")
    print("+++")

def doCommand(command):
    if command.find("spi ") == 0:
        return bs_spi.doCommand(command[4:])
    elif command.find("i2c ") == 0:
        return bs_i2c.doCommand(command[4:])
    elif command.find("uart ") == 0:
        return bs_uart.doCommand(command[5:])
    elif command.find("jtag ") == 0:
        return bs_jtag.doCommand(command[5:])
    elif command == "quit":
        return -1
    else:
        return None

try:
    with open("/tmp/BUSSide.seq", "rb") as f:
        d = f.read(4)
        if len(d) == 4:
            seq, = struct.unpack('<I', d)
            bs.set_sequence_number(seq)
except:
    pass

rv = bs.Connect(device)
if rv is None:
    print("--- Couldn't connect.")
    print("--- Unplug the BUSSide USB cable. Wait a few seconds.")
    print("--- Plug it in again. Then restart the software.")
    sys.exit(1)

print("+++")
print("+++ Welcome to the BUSSide")
print("+++ By Dr Silvio Cesare of InfoSect")
print("+++")
printHelp()
print("+++")

while True:
    try:
        command = raw_input("> ")
    except:
        break
    rv = doCommand(command)
    if rv is None:
        printHelp()
    elif rv == -1:
        break

print("Bye!")

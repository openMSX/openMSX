# RS232Raw Emulation Information

This document describes the `rs232-raw` pluggable, which connects an
emulated RS232 port to a host serial port.

## Overview

The `rs232-raw` RS232 pluggable forwards the RS232 traffic between an MSX
device (UART) and a real serial port of the host operating system. It is the
primary way to connect a real ESP32 (running the ESP32-UNAPI firmware) to
openMSX, for instance through the SM-X WiFi device.

The host serial port is selected with the `rs232-raw-port` setting (default
`COM1` on Windows, `/dev/ttyS0` on Linux).

## Tested RS232 devices

The pluggable has been tested with the following RS232 devices:

* I8251 UART (e.g. the MSXRS232 device)
* SM-X UART (the SM-X WiFi device)
* MSXRS232 device
* SM-X WiFi device

## Compatibility note

The RS232Raw pluggable adjusts the serial port parameters (baud rate, data
bits, stop bits, parity) when the emulated UART issues a speed or mode
change. Other RS232 devices not listed above might need to be adapted to
propagate UART speed updates to the pluggable; otherwise the serial port may
operate at the wrong speed or with incorrect framing parameters.

When in doubt, test with one of the verified devices first.

## IMPORTANT

This might not be time accurate, i.e.: hardware flow control might not work as
expected. Nowadays most of the serial ports are USB converters, and there is a
latency, so changes might be advertised later than when they happened, same when
you set a signal line. This emulation exists only to allow use of software and
hardware for evaluation, it should not be assumed that if an application runs
with this emulation it will run on real hardware, if using for development, always
validate on the real hardware.
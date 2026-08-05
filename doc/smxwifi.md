# SM-X WiFi Emulation Information

This document describes the SM-X WiFi device emulated by openMSX
(`smxwifi`), its I/O interface and how to use it. The SM-X WiFi is built-in
on SM-X computers and on some OCM FPGA computers, not an external cartridge.

## Overview

The `smxwifi` device emulates the DucaSP SM-X WiFi (2020), the MSX-side part
of the SM-X WiFi network adapter: an I/O-mapped UART interface between the
MSX and an external ESP WiFi module. The module itself is **not** emulated
by this device: it is attached through an RS232 connector, so the UART link
can be connected to real hardware or inspected while debugging.

MSX software uses the adapter through the SM-X WiFi UNAPI BIOS driver, which
implements the TCP-IP UNAPI specification over the UART protocol spoken by
the ESP32 firmware.

## UNAPI BIOS driver

The device does not ship a ROM of its own: like the original device, the
driver is loaded by the user and the device only provides the UART
interface. To add UNAPI capabilities to the MSX, download `ESPUNAPI_IO.rom`
from the [UNAPI_BIOS_CUSTOM_ESP_V2](https://github.com/ducasp/MSX-Development/tree/master/UNAPI_BIOS_CUSTOM_ESP_V2)
folder and insert it in any free slot, for example:

```
carte ESPUNAPI_IO.rom
```

Once loaded, the driver appears as a TCP-IP UNAPI implementation and all
UNAPI-compatible software (FTP client/server, telnet, HTTP, ...) can use it.

## ESP32 firmware

The ESP module must run the ESP32-UNAPI firmware, which is built from the
[ESP32-UNAPI-Firmware](https://github.com/ducasp/ESP32-UNAPI-Firmware)
repository and flashed to the ESP32. We recommend using an **ESP32-C6** or
**ESP32-S3** board with **at least 8 MB of flash; 16 MB is recommended**.

## I/O ports

The extension (`smxwifi.xml`) occupies 2 I/O ports at base `0x06`:

| Port     | Direction | Function                                             |
| -------- | --------- | ---------------------------------------------------- |
| base     | read      | UART RX FIFO byte (from the ESP module)              |
| base     | write     | UART command (baud-rate select, FIFO reset)          |
| base + 1 | read      | UART status                                          |
| base + 1 | write     | UART TX byte (to the ESP module)                     |

UART status bits:

| Bit | Meaning                                            |
| --- | -------------------------------------------------- |
| 0   | RX data available in the FIFO                      |
| 3   | Quick receive supported                            |
| 4   | Underrun (read of an empty FIFO), cleared by read  |

Reading the data port waits a short time (about 30 ms) for the ESP module to
provide data; if none arrives, `0xFF` is returned and the underrun bit is set.

UART command values (write to the data port base):

| Value | Function                                   |
| ----- | ------------------------------------------ |
| 0-9   | Select baud rate (see table below)         |
| 20    | Reset the RX FIFO                          |

Baud rates (value = index):

| Value | Baud rate | Value | Baud rate |
| ----- | --------- | ----- | --------- |
| 0     | 859372    | 5     | 38400     |
| 1     | 346520    | 6     | 31250     |
| 2     | 231014    | 7     | 19200     |
| 3     | 115200    | 8     | 9600      |
| 4     | 57600     | 9     | 4800      |

The default after reset is baud rate 0 (859372). The UART parameters are
always 8 data bits, 1 stop bit, no parity.

## Connector and pluggables

The device exposes an RS232 connector named `smxwifi`. The **proper way** to
use the device is to plug `rs232-raw` into it, connecting the UART to a real
ESP32 running the ESP32-UNAPI firmware (or another compatible module) on a
host serial port. Any other RS232 configuration is untested and not
guaranteed to work:

* `rs232-raw`: connects the UART to a host serial port (setting
  `rs232-raw-port`), e.g. a real SM-X WiFi / ESP32 device. This is the
  supported way of using the device.
* `rs232-tester`: reads the UART input from a file (`rs-232-inputfilename`
  setting) and logs the output to a file (`rs232-outputfilename` setting);
  useful to inspect the traffic between the driver and the ESP module.
* `rs232-net`: connects the UART to a host IP:port (`rs232-net-address`
  setting).
* Nothing: the connector stays unplugged (dummy device), the adapter does
  nothing.

Example:

```
ext smxwifi
plug smxwifi rs232-raw
set rs232-raw-port COM7
```

With `rs232-raw` the networking happens on the attached real device, so the
host networking considerations of the GenericUNAPI device (firewall, ports,
TLS) do not apply here.

## SM-X WiFi vs GenericUNAPI device

Both devices present the same I/O interface to the MSX (same port layout,
same baud-rate table; the GenericUNAPI I/O layout is kept compatible with
SM-X WiFi I/O):

* The `genericunapi` device emulates the complete ESP32-UNAPI firmware
  inside openMSX (host networking, DNS, TLS) and needs no plugging; use it
  for full emulation on the host's network stack. See
  [genericunapi.md](genericunapi.md) for its settings and the host
  networking requirements (firewall, privileged ports, SNTP port
  limitations).
* The `smxwifi` device only emulates the MSX-side interface and requires an
  external ESP module on a serial connection (recommendation: use an ESP32 C6
  or S3 module with 16MB flash rom, they come with their own USB to Serial converter).
  Using a real ESP32 makes it easier to bypass host operating-system limitations such
  as the firewall, privileged ports and ports occupied by host services, since the 
  networking then happens on the ESP32 itself.

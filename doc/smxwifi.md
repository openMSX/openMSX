# SM-X WiFi Emulation Information

This document describes the SM-X WiFi device emulated by openMSX
(`smxwifi`), its I/O interface and how to use it. The SM-X WiFi is built-in
on SM-X computers and on some OCM FPGA computers, not an external cartridge.

## Overview

The `smxwifi` device emulates the DucaSP SM-X WiFi (2020): an I/O-mapped
UART interface between the MSX and an ESP WiFi module, where the ESP side
runs the ESP32-UNAPI firmware. The UART link is either:

* **a real ESP32** on a host serial port, attached through the device's
  RS232 connector with `rs232-raw`, or
* **an emulated ESP32** inside openMSX, when no real ESP32 is reachable.

The device switches between the two automatically: while the RS232 connector
has no `rs232-raw` device plugged in (or its host serial port is not open,
e.g. no port selected), the MSX talks to the emulated ESP32. As soon as a
valid `rs232-raw` selection is present and its port can be opened,
communication is switched over to the real ESP32 and the current UART
parameters are applied to the port. Switching in either direction closes all
emulated network connections and starts the emulation clean.

MSX software uses the adapter through the SM-X WiFi UNAPI BIOS driver, which
implements the TCP-IP UNAPI specification over the UART protocol spoken by
the ESP32 firmware.

## UNAPI BIOS driver

To add UNAPI capabilities to the MSX, download `ESPUNAPI_IO.rom`
from the [UNAPI_BIOS_CUSTOM_ESP_V2](https://github.com/ducasp/MSX-Development/tree/master/UNAPI_BIOS_CUSTOM_ESP_V2)

It is free for use and required for this device.

## ESP32 firmware

A real ESP32 module must run the ESP32-UNAPI firmware, which is built from the
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
always 8 data bits, 1 stop bit, no parity. With the emulated ESP32 the baud
rate is not considered (there is no real serial communication).

## Connector and pluggables

The device exposes an RS232 connector named `smxwifi`. If you plan to use a
real ESP32 module instead of the emulated one, plug `rs232-raw` into it to
connect the UART to a real ESP32 (or another compatible module) on a host
serial port:

```
ext smxwifi
plug smxwifi rs232-raw
set rs232-raw-port COM7
```

As long as the connector has no `rs232-raw` device plugged in, or its host
serial port is not open (no port selected, port in use, ...), the device
talks to the emulated ESP32, so the adapter keeps working with no real
hardware attached. Plugging a device other than `rs232-raw` (e.g.
`rs232-tester`, useful to inspect the traffic between the driver and the ESP
module) also leaves the emulated ESP32 in charge.

Note: changes can be done pretty easily on the GUI in the connectors menu.

With `rs232-raw` the networking happens on the attached real device, so the
host networking considerations of the emulated ESP32 (firewall, ports, TLS)
do not apply.

## Emulated ESP32

The emulated ESP32 implements the complete TCP-IP UNAPI firmware inside
openMSX: the TCP-IP UNAPI specification 1.1 (TCP and UDP connections, DNS
resolution, TLS/SSL over TCP active connections) using the network stack of
the host operating system. It DOES NOT emulate SSH UNAPI, that is available
only attaching a real ESP32.

### TLS/SSL support

TLS (spec 1.1, "Use TLS" flag in `TCPIP_TCP_OPEN`) is supported for TCP
**active** connections. The emulation uses the OpenSSL runtime library
**installed on the host operating system**, which is loaded dynamically at
startup.

* If OpenSSL is found, the "_Use TLS in TCP active connections_" capability
  bit is advertised and TLS connections can be opened. The OpenSSL version
  is printed to the openMSX console at startup.
* If OpenSSL is not found, the capability bit is **not** advertised and a
  `TCPIP_TCP_OPEN` with the TLS flag returns `ERR_NOT_IMP`. A message is
  printed to the openMSX console at startup.

TLS in **passive** connections is not implemented (the "_Use TLS in TCP
passive connections_" capability bit is never advertised, and requesting it
returns `ERR_NOT_IMP`, as the specification requires).

### Certificate verification

The "Verify the server certificate" flag of `TCPIP_TCP_OPEN` is honored: the
server certificate is validated against the root certificates of the host
operating system:

* **Windows:** the Windows system certificate store (`ROOT`) is used.
* **Linux:** the default OpenSSL system trust store is used
  (`/etc/ssl/certs` and friends).
* **macOS:** the `/etc/ssl/cert.pem` bundle is used (this file is provided by
  the system).

The server host name sent in the `TCPIP_TCP_OPEN` command is used for SNI
and, when verification is enabled, to validate the host name in the
certificate (matching the way the ESP32-UNAPI firmware reads the host name
from the command data). When no host name is supplied, the certificate chain
is still validated, but the host name is not checked.

A failed TLS handshake or certificate validation closes the connection; the
close reason (spec 4.5.4, reasons 9-19) is reported by `TCPIP_TCP_STATE` as
`ERR_NO_CONN` with the reason code.

### Installing OpenSSL

* **Linux:** OpenSSL is almost always pre-installed (package `openssl` /
  `libssl`). Nothing to do.
* **Windows:** install a Win64 OpenSSL build, e.g. the one distributed by
  [slproweb.com](https://slproweb.com/products/Win32OpenSSL.html) or the
  "OpenSSL" builds by [FireDaemon](https://firedaemon.com) (which are based
  on LibreSSL and use `libssl-4-x64.dll` / `libcrypto-4-x64.dll`). OpenSSL
  1.1.x, 3.x and LibreSSL 3.x/4.x are all supported. Make sure the OpenSSL
  DLLs (`libssl-*.dll` / `libcrypto-*.dll`) are in `PATH` or in the openMSX
  executable directory.
* **macOS:** install via Homebrew: `brew install openssl@3`.

### Network adapter preference

The IP address the emulated ESP32 reports to the MSX (`TCPIP_GET_IPINFO`) is
the one other machines use to reach the MSX, so it must be reachable on the
network. When the host has several adapters, the primary physical one is
chosen:

1. **Wired** (Ethernet) — preferred whenever it is up.
2. **Wireless** (Wi-Fi) — used when no wired adapter is up.
3. **Other physical** adapters (PPP, modems, ...).
4. **Virtual adapters** — only used when no physical adapter has a usable
   IPv4 address, so the emulation still works on machines that only have a
   virtual adapter (e.g. WSL-only setups).

The loopback adapter and adapters that are down are never used. Tunnels
(6to4, ISATAP, Teredo) and VPN software (Tailscale, ZeroTier, WireGuard, ...)
count as virtual.

Virtual adapters are recognized by their type and name:

* **Windows:** tunnel adapters, and adapter names containing `wsl`,
  `vEthernet`, `Hyper-V`, `virtual`, `VMware`, `VirtualBox`, `Tailscale`,
  `ZeroTier`, `Docker`, `tap-`, `isatap` or `teredo` (case-insensitive).
* **Linux/macOS:** interface names starting with `docker`, `veth`, `virbr`,
  `br-`, `tun`, `tap`, `utun`, `awdl`, `bridge`, `tailscale`, `zt`, `wg`,
  `vmnet` or `vbox`.

When adapters tie in preference, the first one is used.

### Host ports and operating-system services

The emulation binds sockets on the **host** operating system to emulate the
network interface of the ESP32. Consequently, a port that is already in use
on the host cannot be used by the device:

* The emulation reports the host's own network configuration to the MSX (its
  primary IPv4 address, netmask, gateway and DNS servers, read fresh on each
  `TCPIP_GET_IPINFO` call; see *Network adapter preference* above), so other
  machines on the same network reach the MSX at that address, without any
  configuration on those machines. When the host has no usable IPv4
  configuration the fields report `0.0.0.0`, except the primary DNS which
  falls back to the well-known `8.8.8.8` (DNS lookups are performed on the
  host anyway, so the reported value is informational).
* Opening a connection (TCP or UDP) on a local port that is occupied by a
  host application returns `ERR_CONN_EXISTS` (or fails).
* In particular, well-known ports used by operating-system services are not
  available. On Windows, the Windows Time service (W32Time) is started by
  default and keeps UDP port 123 in use permanently, so the
  `TCPIP_SNTP_SET`/SNTP functionality cannot use its standard port. To use
  the SNTP port, the W32Time service must be stopped (see below).
* To free a port for the device, stop the host service that occupies it. On
  Windows, stop the Windows Time service temporarily with
  `net stop w32time`, or permanently by setting its startup type to
  "Manual"/"Disabled" in the Services manager (`services.msc`) or with
  `sc config w32time start= disabled`.

### Firewalls and inbound connections

The emulation binds sockets on the host operating system, so inbound traffic
to the emulated MSX has to pass the host's firewall. This applies to servers
running on the MSX (for example an FTP server on port 21) and to
"connect-back" connections, where a remote machine connects to the MSX's
advertised port (for example the data connection of an FTP client). Outbound
connections are normally not affected.

* **Linux (ufw):** ufw denies inbound connections by default. Allow the
  ports used by the MSX applications; for example, for an FTP server and
  the data connections of an FTP client:

      sudo ufw allow from 192.168.0.0/24 to any port 21 proto tcp         # FTP server (control)
      sudo ufw allow from 192.168.0.0/24 to any port 113 proto tcp        # FTP client (IDENT listener)
      sudo ufw allow from 192.168.0.0/24 to any port 1024:65535 proto tcp # FTP data connections

  (the FTP client opens its data connection on a random port in the range
  16384-32767; the FTP server opens its data ports starting at 1024).

  UDP traffic is subject to the same inbound deny, and replies to MSX UDP
  clients are affected as well: TFTP and SNTP on the MSX are clients, so
  their requests go out, but the replies (and subsequent data packets)
  arrive on the host as inbound UDP traffic and are dropped unless allowed.
  Allow the local ports those clients bind - SNTP uses UDP port 123, the
  TFTP client uses port 69 or a random port in the 16384-32767 range:

      sudo ufw allow from 192.168.0.0/24 to any port 123 proto udp        # SNTP client (local port)
      sudo ufw allow from 192.168.0.0/24 to any port 69 proto udp         # TFTP client (local port)
      sudo ufw allow from 192.168.0.0/24 to any port 16384:32767 proto udp # TFTP client (random local port)

  When in doubt, allow the local port(s) the application uses. Port 69 is
  privileged like port 21 (see below).

  For a quick test the firewall can be disabled temporarily with
  `sudo ufw disable` and re-enabled with `sudo ufw enable`. iptables- or
  nftables-based firewalls need equivalent rules.

* **Privileged ports:** on Linux and macOS, binding a port below 1024
  requires root privileges. If an MSX application uses such a port (for
  example an FTP server on port 21), either run openMSX as root, or grant
  the capability once so regular users can bind these ports:

      sudo setcap 'cap_net_bind_service=+ep' /path/to/openmsx

  (the capability must be reapplied after every openMSX update, and the
  filesystem must support file capabilities). Alternatively,
  `authbind --deep openmsx` can be used. Windows has no privileged ports.

## Setting: simulated WiFi link

The `smxwifi-emulatedesp32-link-enabled` setting (default `true`) simulates
the WiFi link state of the emulated ESP32: with the setting disabled, the
emulated ESP32 reports its network link as closed (which is what
`TCPIP_NET_STATE` reports), like a real ESP32 whose WiFi radio is off. It
does not affect a real ESP32 attached through `rs232-raw`.
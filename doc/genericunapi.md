# GenericUNAPI Emulation Information

This document describes the TCP-IP UNAPI device emulated by openMSX
(`genericunapi`), its TLS/SSL support and the operating-system requirements.

## Overview

The GenericUNAPI device emulates both the SM-X UART and an ESP32 with UNAPI
firmware installed, resulting in a network adapter (a TCP-IP UNAPI compliant
network interface) using the network stack of the host operating system. It
implements the TCP-IP UNAPI specification 1.1, including TCP and UDP connections,
DNS resolution and TLS/SSL over TCP active connections.

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

## I/O ports

This extension occupies 2 I/O ports at base `0x06`:

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
always 8 data bits, 1 stop bit, no parity. Note that this device emulates
the ESP module so baud rate is not considered as there is no real serial
communication.

## TLS/SSL support

TLS (spec 1.1, "Use TLS" flag in `TCPIP_TCP_OPEN`) is supported for TCP
**active** connections. The device uses the OpenSSL runtime library **installed
on the host operating system**, which is loaded dynamically at startup.

* If OpenSSL is found, the "_Use TLS in TCP active connections_" capability bit
  is advertised and TLS connections can be opened. The OpenSSL version is
  printed to the openMSX console at startup.
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

The server host name sent in the `TCPIP_TCP_OPEN` command is used for SNI and,
when verification is enabled, to validate the host name in the certificate
(matching the way the ESP32-UNAPI firmware reads the host name from the command
data). When no host name is supplied, the certificate chain is still validated,
but the host name is not checked.

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

## Host ports and operating-system services

The device binds sockets on the **host** operating system to emulate the
network interface of the ESP32. Consequently, a port that is already in use on
the host cannot be used by the device:

* The device reports the host's own network configuration to the MSX (its
  primary IPv4 address, netmask, gateway and DNS servers, read fresh on each
  `TCPIP_GET_IPINFO` call), so other machines on the same network reach the
  MSX at that address, without any configuration on those machines. When the
  host has no usable IPv4 configuration the fields report `0.0.0.0`, except
  the primary DNS which falls back to the well-known `8.8.8.8` (DNS lookups
  are performed on the host anyway, so the reported value is informational).
* Opening a connection (TCP or UDP) on a local port that is occupied by a host
  application returns `ERR_CONN_EXISTS` (or fails).
* In particular, well-known ports used by operating-system services are not
  available. On Windows, the Windows Time service (W32Time) is started by
  default and keeps UDP port 123 in use permanently, so the
  `TCPIP_SNTP_SET`/SNTP functionality cannot use its standard port; the SNTP
  client reports that a resident UDP connection uses the SNTP port. To use
  the SNTP port, the W32Time service must be stopped (see below).
* To free a port for the device, stop the host service that occupies it. On
  Windows, stop the Windows Time service temporarily with
  `net stop w32time`, or permanently by setting its startup type to
  "Manual"/"Disabled" in the Services manager (`services.msc`) or with
  `sc config w32time start= disabled`.

## Firewalls and inbound connections

The device binds sockets on the host operating system, so inbound traffic to
the emulated MSX has to pass the host's firewall. This applies to servers
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

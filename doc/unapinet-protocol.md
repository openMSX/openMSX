# UnapiNet bridge protocol

This document describes the private wire protocol between the Z80 MSX-UNAPI
TCP/IP driver (`unapinet.com`) and the `UnapiNet` device in openMSX. It is
*not* the MSX-UNAPI specification: UNAPI defines what MSX software sees when
it calls the driver; everything below is how the driver talks to the bridge
behind the scenes. The driver and the device are developed and released
together - the compatibility notes below rely on that - but only the device
lives in the openMSX tree.

The C++ implementation is `src/unapinet/UnapiNet.cc`; the byte-exact layouts
of every parameter and result block are the structs in
`src/unapinet/UnapiNetWire.hh`. Where a struct name appears below, that
struct *is* the layout.

Protocol version described here: **v1** (bridge version byte 4, as reported
by `QUERY_CAP`). Known inconsistencies of v1, and the plan to fix them in
v2, are collected at the end.

## I/O ports

The device claims two consecutive I/O ports, base 0x28 (`share/extensions/
unapinet.xml`, `<io base="0x28" num="2"/>`). Decoding is on bit 0 of the
port number:

| Port | Write            | Read                          |
|------|------------------|-------------------------------|
| 0x28 | command (opcode) | status register               |
| 0x29 | parameter byte   | result byte (consuming)       |

## Transaction model

Every operation is one synchronous transaction:

1. The MSX writes the parameter block, one byte at a time, to port 0x29.
2. The MSX writes the command opcode to port 0x28. The device processes the
   command *inside that I/O write*: by the time the OUT instruction
   completes, the result is ready. The MSX never has to wait or poll for
   command completion. (Operations that take real time - DNS lookups,
   connection establishment, ICMP echo - return immediately and are polled
   with their own dedicated commands; see "Asynchronous operations".)
3. The MSX reads the status register (port 0x28). `0x02` (`STATUS_DATA`)
   means a result is waiting.
4. The MSX reads the result, one byte per IN, from port 0x29. Each read
   consumes one byte; reading past the end returns 0x00.

Bookkeeping rules, all enforced by the device:

* The parameter buffer is always cleared after a command executes, whether
  it succeeded or not.
* Writing a parameter byte while a result is still pending discards the
  unread result (a new transaction implicitly abandons the old one). This is
  the recovery path if the MSX program is interrupted mid-read: the next
  transaction starts clean.
* The parameter buffer is capped (64 KiB + 16). Excess bytes are dropped;
  the command's own size check then reports an error. A runaway MSX program
  cannot exhaust host memory.

### Status register values

| Value | Name         | Meaning                                          |
|-------|--------------|--------------------------------------------------|
| 0x00  | STATUS_OK    | idle; no result pending                          |
| 0x01  | STATUS_ERROR | protocol-level error (see below)                 |
| 0x02  | STATUS_DATA  | a result is pending on the data port             |

`STATUS_ERROR` is raised in exactly three situations: unknown opcode,
`DNS_QUERY` with an empty host name, and `DNS_QUERY` while a lookup is
already running. Every other error is reported *inside* a normal result
block. In practice the driver only ever performs one check on this
register: "did the command I just issued produce a result?"
(`status == STATUS_DATA`).

## Detection handshake

The driver detects the device at installation time:

1. `PING` (0x00, no parameters) must answer `STATUS_DATA` with the single
   magic byte **0xAB**. Anything else means the extension is not present.
2. `QUERY_CAP` (0x10, no parameters) answers two bytes: a capability
   summary (0x0F = PING + DNS + TCP + UDP) and the bridge version (4).

*v1 quirk:* the current driver only performs step 1; nothing reads the
version byte today.

## Data types

* **IPv4 address** - 4 bytes, the octets of `a.b.c.d` in order
  (network/big-endian; `Endian::UA_B32` in the structs).
* **port / length / id / seq** - 2 bytes, low byte first (little-endian,
  Z80-natural; `Endian::UA_L16`).
* **handle** - 1 byte. TCP and UDP handles are independent ranges, both
  1-based on the wire: 1..4 (`MAX_TCP` = `MAX_UDP` = 4). In results, handle
  0 means "could not open".

## Command reference

| Op   | Name        | Parameters (struct)            | Result (struct)                  |
|------|-------------|--------------------------------|----------------------------------|
| 0x00 | PING        | none                           | 1 byte: 0xAB                     |
| 0x01 | DNS_QUERY   | hostname bytes, NUL-terminated | see "Asynchronous operations"    |
| 0x02 | DNS_STATUS  | none                           | see "Asynchronous operations"    |
| 0x03 | TCP_OPEN    | `TcpOpenParams` (11)           | 1 byte: handle, 0 = failed       |
| 0x04 | TCP_SEND    | `TcpSendParamHeader` (3) + payload | 1 byte: 0 = accepted, 1 = error, 2 = send buffer full |
| 0x05 | TCP_RECV    | `TcpRecvParams` (3)            | `TcpRecvResultHeader` (2) + payload |
| 0x06 | TCP_CLOSE   | 1 byte: handle (0 = all transient) | 1 byte: 0 = OK, 1 = error    |
| 0x07 | TCP_STATE   | 1 byte: handle                 | `TcpStateResult` (12)            |
| 0x08 | TCP_ABORT   | 1 byte: handle                 | 1 byte: 0 = OK, 1 = error        |
| 0x09 | UDP_OPEN    | `UdpOpenParams` (2)            | 1 byte: handle, 0 = failed       |
| 0x0A | UDP_CLOSE   | 1 byte: handle (0 = all transient) | 1 byte: 0 = OK, 1 = error    |
| 0x0B | UDP_STATE   | 1 byte: handle                 | `UdpStateResult` (2)             |
| 0x0C | UDP_SEND    | `UdpSendParamHeader` (9) + payload | 1 byte: 0 = OK, 1 = error    |
| 0x0D | GET_LOCALIP | none                           | 4 bytes: host IPv4               |
| 0x0E | NET_STATE   | none                           | 1 byte: always 2 ("open")        |
| 0x0F | UDP_RECV    | `UdpRecvParams` (3)            | `UdpRecvResultHeader` (8) + payload |
| 0x10 | QUERY_CAP   | none                           | 2 bytes: caps (0x0F), version (4) |
| 0x11 | ICMP_SEND   | `IcmpSendParams` (11)          | 1 byte: 0 = queued, 1 = error    |
| 0x12 | ICMP_RECV   | none                           | 1 byte: 0 = none, or `IcmpRecvResult` (12) |

Any other opcode raises `STATUS_ERROR`.

### TCP

**TCP_OPEN** - `TcpOpenParams`: remote IP (4), remote port (2), local
port (2), timeout (2, *reserved: never read*), flags (1; bit 0 = passive
open, bit 1 = resident). An active open starts a non-blocking `connect()`
and returns the handle immediately; the driver polls `TCP_STATE` until the
state leaves SynSent. A passive open (bit 0) listens on the local port;
remote IP, if non-zero, filters which peer may connect.

**TCP_SEND** - header {handle (1), length (2)} followed by exactly `length`
payload bytes. The driver keeps to 4096 bytes (`MAX_TRANSFER`) per command
by convention, but the bridge does not enforce that bound on the send side
(a v1 quirk; only TCP_RECV clamps to it). Result 2 ("send buffer full") is
a recoverable condition: the bridge queues up to 64 KiB (`MAX_SEND_BUF`)
that the kernel has not yet accepted, and the driver should retry later.
Result 1 is a real failure (bad handle/state/length, or a connection error
while queueing).

**TCP_RECV** - {handle (1), maxlen (2)}. Answers {actualLen (2)} followed
by `actualLen` bytes taken from the connection's receive buffer, at most
4096 per command. `actualLen` 0 means "nothing buffered right now" - poll
`TCP_STATE` to distinguish an empty buffer on a live connection from a
finished one. *v1 quirk:* a bad handle also answers `actualLen` 0.

**TCP_STATE** - answers `TcpStateResult`: state (1), bytes available to
read (2), close reason (1), remote IP (4), remote port (2), local
port (2). An invalid or missing handle answers all-zero fields with
closeReason = 1 (NeverUsed).

State values (TCP-standard names):
0 Closed, 1 Listen, 2 SynSent, 3 SynRecv, 4 Established, 5 FinWait1,
6 FinWait2, 7 CloseWait, 8 Closing, 9 LastAck, 10 TimeWait.

Close reasons: 0 None, 1 NeverUsed, 2 ClosedByUser, 3 Aborted,
4 ConnectionReset, 6 ConnectFailed.

**TCP_CLOSE vs TCP_ABORT** - CLOSE is graceful: the connection enters
FinWait1, the FIN goes out once everything the MSX queued has actually
been sent, and the handle is freed when the peer closes too - or after a
30 s timeout, so a silent peer cannot pin a handle forever. Received data
still buffered stays readable meanwhile. ABORT drops the connection
immediately. Handle 0 in TCP_CLOSE (and UDP_CLOSE) closes every open
non-resident connection at once - the driver uses it on program exit;
resident connections (TCP_OPEN flags bit 1) survive it.

### UDP

**UDP_OPEN** - local port (2), 0xFFFF = ephemeral. **UDP_STATE** answers
the size of the first queued datagram (2 bytes; 0 = queue empty, or bad
handle - same v1 quirk as TCP_RECV). **UDP_SEND** - header {handle (1),
destination IP (4), destination port (2), length (2)} + payload; result 1
also covers a short kernel write. **UDP_RECV** - {handle (1), maxlen (2)};
answers {source IP (4), source port (2), actualLen (2)} + payload.
Datagram semantics apply: if the first queued datagram is larger than
`maxlen`, the excess of *that datagram* is discarded.

### ICMP

**ICMP_SEND** - {destination IP (4), TTL (1), identifier (2),
sequence (2), size (2, clamped to 512)}. No payload follows: the bridge
fabricates the echo data itself. The request is queued to a worker thread;
the result byte only acknowledges the queueing. **ICMP_RECV** polls for
replies: 1 byte 0 = none yet, otherwise {1, source IP (4), TTL (1),
identifier (2), sequence (2), dataLen (2)} - header only, the echo payload
is not returned. *Current limitation:* the ICMP worker is implemented with
the Windows ICMP API; on other host platforms ICMP_RECV never reports
data.

## Asynchronous operations

**DNS.** `DNS_QUERY` takes the hostname as its parameter block,
NUL-terminated (the terminator and anything after it are ignored). If the
name parses as a dotted-quad IP it answers immediately with 5 bytes
{1, ip}. Otherwise it starts a resolver thread and answers a single byte 0
("started"); the driver then polls `DNS_STATUS`, which answers one byte
0/1 (idle / in progress), 5 bytes {2, ip} on success, or 2 bytes
{0xFF, 3} on failure (the only failure reported is "no such host", UNAPI
error 3). The completed status is sticky: it stays readable until the next
`DNS_QUERY` or a device reset. Issuing `DNS_QUERY` while a lookup is
running raises `STATUS_ERROR` without disturbing the running lookup.

**TCP connect** is polled via `TCP_STATE` (SynSent → Established or
Closed + ConnectFailed).

**Receive path.** A background thread moves incoming TCP data into a
per-connection buffer of up to 64 KiB (`MAX_RECV_BUF`), from which
`TCP_RECV` reads. When the buffer is full the bridge stops reading from
the socket, the kernel buffer fills, and TCP flow control throttles the
peer - no TCP data is ever dropped. UDP is best-effort, as datagrams are:
they are read into a 2 KiB buffer (larger ones are truncated at receive
time) and queued per socket up to 16 deep; beyond that, new arrivals are
read and discarded.

## Known v1 inconsistencies, and the v2 plan

These grew one command at a time and are preserved here so the next reader
does not have to rediscover them:

1. **Three error conventions coexist.** OPEN commands answer a handle
   where 0 = failure; most other commands answer 0 = OK / 1 = error (and
   TCP_SEND adds 2 = buffer full); `STATUS_ERROR` covers only the three
   protocol-level cases listed above.
2. **"No data" and "bad handle" are indistinguishable** in TCP_RECV,
   UDP_STATE and UDP_RECV.
3. **Parameter blocks are validated as "at least this long"**, not exactly;
   trailing garbage is silently accepted.
4. **`QUERY_CAP` is implemented but never issued** by the driver, so the
   version byte is dead on the wire.
5. **`NET_STATE` answers a constant.**
6. **`TcpOpenParams.timeout` is never read.**
7. **`MAX_TRANSFER` is enforced on TCP_RECV but not on TCP_SEND** - the
   4096-byte bound on sends is a driver-side convention the bridge
   silently trusts.

The planned v2, coordinated with the driver (both halves ship from the
same repository): every reply starts with a status byte (0 = OK, otherwise
the UNAPI error code the driver should return, `ERR_BUFFER` included);
handles stop doubling as error codes; parameter blocks are validated for
exact size; the PING magic is bumped so an old driver fails detection
cleanly instead of misbehaving quietly; and the driver reads `QUERY_CAP`
for real. With a status byte on every reply the status register becomes
redundant (the driver's only use of it is the `== STATUS_DATA` check after
each command); whether to repurpose or retire it is an open design point.

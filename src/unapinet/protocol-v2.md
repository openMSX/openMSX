# UnapiNet bridge protocol v2 — design proposal

Status: **proposal, not implemented**. This document exists to agree on the
framing before it turns into code. The protocol as it exists today (v1) is
described in `doc/unapinet-protocol.md`, which deliberately stays untouched
while this proposal is under review — it documents the code you can read
right now. Once v2 lands, that document gets folded into this one as a
historical appendix, and this file becomes the reference.

The driver (`unapinet.com`, Z80) and this device ship from the same source
project and are updated together. v1 has four users, all personally
reachable; there is no installed base to migrate. v2 is therefore a clean
break, with detection designed so that any old/new mismatch fails cleanly
at installation time (analysis, and one caveat, at the end).

## Design rules

v1's problems are catalogued at the end of the v1 document; they reduce to
one sentence: *every command grew its own conventions*. v2 replaces them
with four rules:

1. **Every reply begins with a status byte.** `0x00` = success. Any other
   value is the MSX-UNAPI TCP/IP error code the driver should hand to its
   caller, verbatim. The device speaks UNAPI error codes natively; the
   driver stops translating.
2. **An error reply is exactly one byte** — the status. No partial
   payloads: if the status is non-zero, there is nothing else to read.
3. **Parameter blocks are validated for exact size.** Too short, too long,
   or inconsistent with a declared payload length → `ERR_INV_PARAM`, and
   the command has no effect.
4. **No valid status byte is 0xFF.** Reading the data port past the end
   of a reply (or with no reply pending) returns `0xFF` — the same value
   an absent device yields from the bus pull-ups, and never a valid
   status (success is `0x00`, and no UNAPI error code is `0xFF`). A
   driver reading a status position and seeing `0xFF` knows it is
   desynchronized or talking to nothing — never "OK". (v1 returned `0x00`
   there, which is indistinguishable from success — Wouter's observation.)
   Bytes *inside* a reply are length-delimited and unconstrained: an IP
   octet, a payload byte or the DNS failed-lookup marker may legitimately
   be `0xFF`.

### Error codes on the wire

The status byte uses the MSX-UNAPI TCP/IP error space. The device emits
this subset (the values match the error table the driver already defines;
the device side is what this proposal adds):

| Code | Name            | Emitted when                                    |
|------|-----------------|-------------------------------------------------|
| 0    | ERR_OK          | success                                          |
| 1    | ERR_NOT_IMP     | unknown opcode; the ICMP commands on a host without working ICMP (caps bit4 clear) |
| 2    | ERR_NO_NETWORK  | the host socket layer refused: socket()/bind()/listen() failure, a synchronous connect() failure, a failed or short sendto() (UDP) |
| 3    | ERR_NO_DATA     | UDP_RECV / ICMP_RECV with an empty queue         |
| 4    | ERR_INV_PARAM   | malformed parameter block (rule 3), empty hostname, a TCP_SEND length beyond `MAX_TRANSFER`, an undefined flag bit set, UDP_RECV with maxlen 0 |
| 5    | ERR_QUERY_EXISTS| DNS_QUERY while a lookup is already running      |
| 9    | ERR_NO_FREE_CONN| TCP_OPEN / UDP_OPEN with all handles in use      |
| 11   | ERR_NO_CONN     | handle out of range (0 included, save the CLOSE commands' close-all form); or no open connection on it — except TCP_STATE / TCP_RECV, which answer for any in-range handle (see notes) |
| 12   | ERR_CONN_STATE  | a connection exists but its state forbids the command — including a TCP send whose socket write fails, which resets the connection and answers 12 |
| 13   | ERR_BUFFER      | TCP_SEND with the 64 KiB send queue full (recoverable: retry later; nothing is partially queued) |

Rule 3 applies to **every** opcode, parameterless ones included: a
malformed or stray parameter block answers `{4}` regardless of the
command, and the per-command "err" lists below do not repeat that. Where
a list names 4 explicitly, a *well-formed* block can still be
semantically invalid there. One check precedes rule 3: opcode dispatch —
an unknown opcode answers `{1}` whether or not stray parameter bytes
accompany it (there is no expected size to validate against). In both
cases the parameter buffer is cleared, as after every command.

Validation order is fixed: form first (rule 3, `{4}`, together with the
semantic `{4}`s — an invalid value inside a well-formed block); then
capability (`{1}`, the ICMP commands); then handle (`{11}`); then
connection state (`{12}`, and DNS's busy `{5}`); then the data check
(`{3}`, the RECV queues). The host operation runs last, so its errors
(`{2}`, `{13}` — plus the TCP write-failure `{12}`, the one
post-operation use of that code) imply every earlier check passed. In
the OPEN commands the free-slot check (`{9}`) precedes the host socket
work (`{2}`).

### The status register becomes a mirror

The driver's only use of the v1 status register is "did my command produce
a result?" — with rule 1 that information is the reply's first byte, so
the register is redundant as a mechanism. Reading the command port now
returns the status byte of the **last completed command** (`0xFF` after
reset, before any command has run). It costs nothing, keeps the port
readable for debugging, and means register and first reply byte can never
disagree. Only command completion updates it: parameter writes, result
reads and the discard of a pending result leave it untouched. The three
v1 situations that raised `STATUS_ERROR` in the register become ordinary
single-byte error replies (unknown opcode → 1, empty hostname → 4, DNS
busy → 5).

## What does not change — and two things that do

Unchanged from v1: ports and decoding (base 0x28: command/status, base+1:
parameter/result), the synchronous transaction model, parameter
accumulation, the parameter-buffer cap, opcode numbers (except 0x10,
retired — below), parameter block layouts, handles (1-based, 1..4, TCP
and UDP independent), `handle 0 = close all transient` in the CLOSE
commands, MAX_TRANSFER/MAX_RECV_BUF/MAX_SEND_BUF, the graceful-close
semantics, and the receive-side backpressure. Byte order stays as it is:
IPs as network-order octets, 16-bit quantities little-endian.

Two v1 statements are explicitly superseded — a reader must NOT carry
them over from the v1 document:

* v1's transaction model says reading past the end returns `0x00` and
  gates each command on `status register == STATUS_DATA (2)`. In v2,
  past-end reads return `0xFF` (rule 4) and the register is a mirror; a
  v1-style `== 2` gate would misread success as "no result".
* v1 ignores anything after DNS_QUERY's NUL terminator. v2 rejects it
  (rule 3). This is the only change at the block-form level; no layout
  moved. (Two *value* checks are also new — TCP_OPEN's undefined flag
  bits and UDP_RECV's maxlen 0, both `{4}`: v1 ignored the former and
  destroyed a datagram on the latter.)

The recovery rule is kept and completed: writing a **parameter byte**
while a result is pending discards the unread result (as in v1), and
writing a **command opcode** while a result is pending abandons it too —
the new reply replaces it. A driver never has to drain a reply it has
lost interest in.

## DETECT (0x00) — replaces PING and QUERY_CAP

Wouter suggested merging the two, and the merge solves three things at
once: the confusing "PING" name (it is not ICMP), the version byte nobody
reads, and the magic bump v2 needs anyway.

`DETECT`, opcode 0x00, no parameters. Success reply, 5 bytes:

| Offset | Value                                              |
|--------|----------------------------------------------------|
| 0      | status 0x00                                        |
| 1      | magic **0x55** (was 0xAB in v1)                    |
| 2      | protocol version: **2**                            |
| 3      | capabilities: bit0 DNS, bit1 TCP active, bit2 TCP passive, bit3 UDP, bit4 ICMP, rest reserved 0 |
| 4      | reserved 0                                         |

The driver accepts the device if and only if bytes 0-2 are exactly
`00 55 02`. Bytes 3-4 are data, not part of the acceptance rule: a driver
must tolerate unknown capability bits (and any value in byte 4), or it
will reject future devices for advertising more.

Capabilities are honest and enforced: bit4 (ICMP) is set only when the
host can actually ping — platform support compiled in *and* the ICMP
channel opening successfully at device start (today that means: Windows,
and `IcmpCreateFile()` succeeded); it is latched once at start. If bit4
is clear, the ICMP commands answer `ERR_NOT_IMP`. Bits 0-3 are always set
on this device today (byte 3 reads 0x0F or 0x1F); they are informational,
and no command is gated on them.

One corner is worth a recipe: if a previous program wrote parameter bytes
and crashed before issuing any opcode, the first DETECT sees a stray
block and answers `{4}` — but that failed command clears the buffer, so
the driver simply issues DETECT once more. Detection is therefore:
issue DETECT; on `{4}`, issue it again; apply the acceptance rule.

Opcode 0x10 (QUERY_CAP) is retired and answers `ERR_NOT_IMP` like any
other unknown opcode.

## Command reference (v2 replies)

Parameter layouts are unchanged from v1 (see the v1 document for byte
detail); this table gives the v2 reply, status byte included. Where a row
shows `{0, Struct}`, the v2 wire struct is the v1 struct with the status
byte **prepended** as its new first member — sizes grow by one, and the
count shown already includes it — except ICMP_RECV, where the status
**replaces** v1's `hasData` marker byte (still 12). "err:" lists the
codes each command can emit besides 0 and besides the universal `{4}` of
rule 3.

| Op   | Name        | Success reply (bytes)                    | err          |
|------|-------------|------------------------------------------|--------------|
| 0x00 | DETECT      | {0, 0x55, 2, caps, 0} (5)                | —            |
| 0x01 | DNS_QUERY   | {0, 0} async started, or {0, 1, ip4} (6) resolved immediately | 4, 5 |
| 0x02 | DNS_STATUS  | {0, state, ...} — see DNS below          | —            |
| 0x03 | TCP_OPEN    | {0, handle} (2)                          | 2, 4, 9      |
| 0x04 | TCP_SEND    | {0} (1)                                  | 4, 11, 12, 13|
| 0x05 | TCP_RECV    | {0, len.lo, len.hi, payload...} (3+len)  | 11           |
| 0x06 | TCP_CLOSE   | {0} (1)                                  | 11           |
| 0x07 | TCP_STATE   | {0, TcpStateResult} (13)                 | 11           |
| 0x08 | TCP_ABORT   | {0} (1)                                  | 11           |
| 0x09 | UDP_OPEN    | {0, handle} (2)                          | 2, 9         |
| 0x0A | UDP_CLOSE   | {0} (1)                                  | 11           |
| 0x0B | UDP_STATE   | {0, size.lo, size.hi} (3)                | 11           |
| 0x0C | UDP_SEND    | {0} (1)                                  | 2, 11        |
| 0x0D | GET_LOCALIP | {0, ip4} (5)                             | —            |
| 0x0E | NET_STATE   | {0, 2} (2)                               | —            |
| 0x0F | UDP_RECV    | {0, srcip4, port.2, len.2, payload} (9+len) | 3, 4, 11  |
| 0x11 | ICMP_SEND   | {0} (1)                                  | 1            |
| 0x12 | ICMP_RECV   | {0, srcip4, ttl, id.2, seq.2, len.2} (12)| 1, 3         |

### Handles, slots, and what survives a close

* Handles are 1..4. **Handle 0 is out of range** (`{11}`) everywhere
  except TCP_CLOSE / UDP_CLOSE, where it means "close all transient" —
  and succeeds (`{0}`) even when nothing is open.
* `ERR_NO_CONN` (11) means the handle is out of range — or, for the
  commands that act on a live connection (SEND, CLOSE, ABORT, and all of
  UDP), that no open connection sits on it. **TCP_STATE and TCP_RECV
  answer for any in-range handle, open or closed**: a closed connection
  keeps its final state, its close reason and its undrained receive data
  until TCP_OPEN reuses the slot. That is deliberate v1 behavior, kept on
  purpose — the tail of a connection that died stays readable (TCP_ABORT
  included), and the driver can still see *why* it died. What 11 removes
  is v1's ambiguity: a zeroed struct or a zero length no longer doubles
  as "bad handle".
* A **never-used** in-range slot answers TCP_STATE with closeReason 1
  (NeverUsed) and every other field zero, and TCP_RECV with length 0 — same frame
  as v1, minus the bad-handle overload.
* Slot allocation: lowest-numbered free slot first. A closed slot holding
  a sticky tail **is** free — it does not count toward `{9}`, and reusing
  it destroys the tail. A slot consumed by a failed TCP_OPEN (`{2}`) is
  released, not left in ConnectFailed (that state belongs to the
  *asynchronous* connect path only).

### TCP

* **TCP_SEND** may send in Established and CloseWait; any other existing
  connection state answers `{12}`; no connection at all answers `{11}`.
  A length beyond `MAX_TRANSFER` (4096) answers `{4}`; length 0 is a
  valid no-op (`{0}`). `{13}` is all-or-nothing: nothing was queued, the
  same command may be retried verbatim later. A socket write failure
  resets the connection (closeReason 4, ConnectionReset) and answers
  `{12}`.
* **TCP_RECV**: `maxlen` is a ceiling request — a value beyond 4096 is
  clamped, not an error (contrast TCP_SEND, where length states a fact
  about the payload that follows and must be exact). `maxlen` 0 answers
  `{0, 0, 0}` and consumes nothing. Length 0 with a live connection is
  normal stream behavior; poll TCP_STATE to tell "live but idle" from
  "finished".
* **TCP_CLOSE** is graceful: FinWait1, FIN once everything queued has
  been sent, handle freed when the peer closes too — or after 30 s.
  Buffered received data stays readable throughout (and after, per the
  sticky-slot rule). Closing a Listen handle is valid and frees it. A
  second CLOSE while the close is in progress is idempotent (`{0}`).
  **TCP_ABORT** drops immediately (closeReason 3).
* **TCP_OPEN**: the `timeout` field stays in the layout, is accepted with
  any value, and is ignored (reserved). Undefined flag bits (2-7) must be
  0; a set one answers `{4}`. Remote IP / port are not semantically
  validated — an active open to 0.0.0.0 or port 0 is handed to the host
  and fails there (`{2}` or, asynchronously, ConnectFailed). Passive
  open: a non-zero remote IP filters the peer; a non-matching peer is
  accepted and immediately dropped, and the listener keeps listening; on
  a match the handle *becomes* the connection (Listen → Established —
  the listener is gone, one connection per passive open).

### UDP

* **Every UDP socket is transient.** The UNAPI residency concept has no
  wire representation in UdpOpenParams, so UDP_CLOSE 0 closes every open
  UDP socket. (The device keeps an internal resident flag for future use;
  nothing can set it today.)
* **UDP_CLOSE discards the socket's queued datagrams** — unlike TCP,
  nothing UDP stays readable after close.
* **UDP_SEND**: an oversized datagram is not a form error — the host
  refuses it and the reply is `{2}` (as is a short kernel write). A
  failed send leaves the socket open and usable. Length 0 sends an empty
  datagram (`{0}`).
* **UDP_RECV**: if the head datagram exceeds `maxlen`, the excess of
  *that datagram* is discarded — it is consumed whole either way.
  `maxlen` 0 is refused (`{4}`) rather than silently destroying a
  datagram unread; use UDP_STATE to size the head of the queue.
  Datagrams were already truncated to 2 KiB at receive time and queue at
  most 16 deep (v1 behavior, unchanged).
* **UDP_OPEN**: local port 0xFFFF requests an ephemeral port; local port
  0 reaches the host `bind()` where it also yields an ephemeral port —
  the two coincide by different routes.

### ICMP

* With caps bit4 clear, ICMP_SEND and ICMP_RECV answer `{1}` — v1
  accepted the request, dropped it silently, and let ICMP_RECV report
  "no data" forever; the capability bit becomes enforced, not
  decorative.
* The echo-size field keeps v1's clamp to 512. On a capable host a
  well-formed ICMP_SEND cannot fail: `{0}` acknowledges queueing to the
  worker, not delivery — an unreachable destination simply never yields
  a reply.
* Replies queue FIFO, at most 16 deep; when the queue is full new
  replies are dropped. Each successful ICMP_RECV consumes exactly one.
  The device does not correlate replies with requests — matching
  identifier/sequence is the driver's job.

### Miscellaneous

* **GET_LOCALIP** answers 0.0.0.0 when the host lookup fails (still a
  success reply — v1 parity). On a multi-homed host, which address is
  reported is implementation-defined.
* **NET_STATE** keeps its constant answer (`{0, 2}`); it exists because
  the driver calls it, and inventing semantics for it is not this
  redesign's job.
* In `UnapiNetWire.hh`, every result struct gains `uint8_t status = 0;`
  as its first member — the hardcoding Wouter suggested: the correct
  value is baked into the type, `T{}` produces a valid success frame,
  and the golden-bytes tests verify it like any other field.

## DNS

`DNS_QUERY` — parameters: the hostname bytes followed by a NUL, which
must be the last byte of the block; anything else answers `{4}`. The
device applies no syntax or charset rules to the hostname — beyond the
dotted-quad fast path below, the bytes go to the host resolver as-is;
the only upper length bound is the parameter-buffer cap. A block that
parses as a strict dotted-quad (`a.b.c.d`, four decimal octets 0-255)
resolves immediately → `{0, 1, ip4}`, and arms the sticky Complete state
exactly as an asynchronous success does. Anything else starts the
resolver thread → `{0, 0}`. An empty hostname (a lone NUL) answers `{4}`;
a lookup already running answers `{5}`; both leave the DNS state and any
running lookup untouched.

`DNS_STATUS` (no parameters) reports the lookup, and its reply is data,
not a command error — a well-formed DNS_STATUS always succeeds:

| Reply              | Meaning                                   |
|--------------------|-------------------------------------------|
| {0, 0}             | idle — no lookup since reset              |
| {0, 1}             | in progress                               |
| {0, 2, ip4} (6)    | complete                                  |
| {0, 0xFF, sub} (3) | lookup failed — a success reply carrying bad news; `sub` = UNAPI DNS_S sub-error (today only 3, "no such host") |

The completed/failed state is sticky until the next `DNS_QUERY` or a
device reset, so the driver may re-read it freely. Keeping the failure
inside a successful reply preserves rule 2 (*command* errors are
single-byte) while still carrying the sub-code the driver's callers
display to users.

## Reset

A device reset (power-on, MSX reset) restores the ground state: every
TCP and UDP connection is closed and freed — resident ones included —
all receive buffers, send queues, datagram queues and ICMP replies are
discarded, any pending reply and accumulated parameter bytes are
dropped, the DNS state returns to idle, and the status register reads
`0xFF` until the first command completes. The ICMP capability stays as
latched at device start; reset does not re-probe it.

## Migration and failure analysis

Both halves ship together; the four v1 users get the new `unapinet.com`
with the new openMSX build. The interesting cases are the mismatches:

* **v1 driver, v2 device.** The driver sends opcode 0x00 (its PING) and
  expects the register to read `STATUS_DATA` (2) and the data port to
  yield `0xAB`. The v2 device runs DETECT: the register mirror reads 0x00
  and the data port yields `0x00 0x55 ...`. Both checks fail on the first
  byte → the driver prints "extension not found" and exits. Nothing
  misbehaves quietly.
* **v2 driver, v1 device.** DETECT goes out as opcode 0x00, which v1
  treats as PING and answers `0xAB`. The v2 driver requires the first
  byte to be `0x00` — `0xAB` fails immediately. Same clean exit.
* **No device.** Every read returns `0xFF` from the pull-ups, which v2
  defines as never-valid at a status position. (This is why past-end
  reads also return `0xFF`: "absent device" and "desynchronized driver"
  become the same, safe, diagnosis.)

One caveat for completeness: the analysis above covers detection, i.e.
installation time — the only supported pairing path. A *resident* v1 TSR
carried into a v2 device by other means (a savestate taken on an old
openMSX and restored on a new one) is not protected: v1's per-call check
is `status register == 2`, and the v2 mirror legitimately reads 2 after a
command failed with `ERR_NO_NETWORK`. Re-running the driver's installer
is the answer; the spec simply does not promise more.

## Open points for review

1. The magic value (0x55) and the capability bit assignments are
   proposals — bikeshed away.
2. Whether NET_STATE deserves real semantics or retirement is left for
   after v2; changing two things at once helps nobody.
3. Anything the tables above get wrong about what a command can actually
   fail with — the err column plus the validation order is the contract
   the TSR will be written against, so it is the part that most wants
   review.

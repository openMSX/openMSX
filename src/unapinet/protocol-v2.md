# UnapiNet bridge protocol v2

Status: **implemented — this is the reference.** This document was agreed
as a proposal before it turned into code; what follows is the protocol
the device and the driver (`unapinet.com`, Z80) actually speak, and the
golden-byte tests pin every reply struct's layout to it. As promised while it was
under review, the v1 document it replaces is folded in below as a
historical appendix — still the byte-level reference for the
parameter-block layouts, which v2 keeps. Functional testing added one
clause the proposal had left unstated: UDP_OPEN falls back to an
ephemeral port when the host refuses the requested one (see UDP below).

The driver and this device ship from the same source project and are
updated together. v1 has four users, all personally reachable; there is
no installed base to migrate. v2 is therefore a clean break, with
detection designed so that any old/new mismatch fails cleanly at
installation time (analysis, and one caveat, under "Migration and
failure analysis" below).

## Design rules

v1's problems are catalogued at the end of the v1 appendix; they reduce to
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
the device side is what v2 added):

| Code | Name            | Emitted when                                    |
|------|-----------------|-------------------------------------------------|
| 0    | ERR_OK          | success                                          |
| 1    | ERR_NOT_IMP     | unknown opcode; the ICMP commands on a host without working ICMP (caps bit4 clear) |
| 2    | ERR_NO_NETWORK  | the host socket layer refused: socket()/bind()/listen() failure, a synchronous connect() failure, a failed or short sendto() (UDP) |
| 3    | ERR_NO_DATA     | UDP_RECV / ICMP_RECV with an empty queue         |
| 4    | ERR_INV_PARAM   | malformed parameter block (rule 3), empty hostname, one longer than 253 bytes or one with an embedded NUL, a TCP_SEND length beyond `MAX_TRANSFER`, an undefined flag bit set |
| 5    | ERR_QUERY_EXISTS| DNS_QUERY while a lookup is already running      |
| 9    | ERR_NO_FREE_CONN| TCP_OPEN / UDP_OPEN with all handles in use      |
| 11   | ERR_NO_CONN     | handle out of range (0 included, save the close-all forms of TCP_CLOSE / UDP_CLOSE / TCP_ABORT); or no open connection on it — except TCP_STATE / TCP_RECV, which answer for any in-range handle (see notes) |
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

### The status register is gone — the command port is write-only

The driver's only use of the v1 status register is "did my command produce
a result?" — with rule 1 that information is the reply's first byte, so
the register is redundant as a mechanism, and v2 removes it entirely
rather than keeping a mirror nobody needs (Wouter's suggestion). The
device registers the command port for writes only; reading it decodes to
the open bus (`0xFF` on a standard MSX), exactly as if no device were
present. The status byte lives in one place — the reply — so there is
nothing to poll, nothing that can disagree with it, and nothing extra to
document. The three v1 situations that raised `STATUS_ERROR` in the
register become ordinary single-byte error replies (unknown opcode → 1,
empty hostname → 4, DNS busy → 5).

## What does not change — and two things that do

Unchanged from v1: ports and decoding (base 0x28: command, base+1:
parameter/result — the command port's read side is gone, see above), the
synchronous transaction model, parameter
accumulation, the parameter-buffer cap, opcode numbers (except 0x10,
retired — below), parameter block layouts, handles (1-based, 1..4, TCP
and UDP independent), `handle 0 = close all transient` in the CLOSE
commands (v2 extends the same form to TCP_ABORT — see the handles
notes), MAX_TRANSFER/MAX_RECV_BUF/MAX_SEND_BUF, the graceful-close
semantics, and the receive-side backpressure. Byte order stays as it is:
IPs as network-order octets, 16-bit quantities little-endian.

Two v1 statements are explicitly superseded — a reader must NOT carry
them over from the v1 document:

* v1's transaction model says reading past the end returns `0x00` and
  gates each command on `status register == STATUS_DATA (2)`. In v2,
  past-end reads return `0xFF` (rule 4) and the register does not exist —
  the port reads as open bus (`0xFF`), so a v1-style `== 2` gate can
  never pass.
* v1's DNS_QUERY takes a NUL-terminated name (terminator effectively
  optional, trailing bytes ignored). v2 drops the terminator entirely:
  the parameter block *is* the hostname. This is the only change at the
  block-form level; no other layout moved. (One *value* check is also
  new — TCP_OPEN's undefined flag bits answer `{4}` where v1 ignored
  them. And one value gains meaning: v1 destroyed a datagram unread on
  UDP_RECV maxlen 0; v2 makes that TCPIP_UDP_RCV's deliberate DE=0
  discard, reporting the source and size while copying nothing.)

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
will reject future devices for advertising more. Byte 2 is the upgrade
path: a hypothetical v3 reports 3 there, a v2 driver rejects it cleanly,
and a v3 driver may accept both if v3 is a superset.

Capabilities are honest and enforced: bit4 (ICMP) is set only when the
host can actually ping — platform support compiled in *and* the ICMP
channel opening successfully at device start (today that means: Windows,
and `IcmpCreateFile()` succeeded); it is latched once at start. If bit4
is clear, the ICMP commands answer `ERR_NOT_IMP`. Bits 0-3 are always set
on this device today (byte 3 reads 0x0F or 0x1F); they are informational,
and no command is gated on them.

One corner is worth a recipe: a crashed predecessor may have left stray
parameter bytes buffered, and a strict DETECT would then answer `{4}` —
yet DETECT is also the stream-resynchronization command, so it must work
from any state. Wouter's resolution: keep DETECT strict, and have the
driver issue it **twice unconditionally**, reading only the second
reply. The first either succeeds and is abandoned (issuing a command
discards a pending result — the recovery rule) or answers `{4}` and
clears the stray block; either way the second DETECT runs on a clean
buffer. No conditional on either side.

Opcode 0x10 (QUERY_CAP) is retired and answers `ERR_NOT_IMP` like any
other unknown opcode.

## Command reference (v2 replies)

Parameter layouts are unchanged from v1 (see the v1 appendix for byte
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
| 0x0F | UDP_RECV    | {0, srcip4, port.2, len.2, payload} (9 + min(len, maxlen)) | 3, 11 |
| 0x11 | ICMP_SEND   | {0} (1)                                  | 1            |
| 0x12 | ICMP_RECV   | {0, srcip4, ttl, id.2, seq.2, len.2} (12)| 1, 3         |

### Handles, slots, and what survives a close

* Handles are 1..4. **Handle 0 is out of range** (`{11}`) everywhere
  except TCP_CLOSE, UDP_CLOSE and TCP_ABORT, where it means "close (or
  abort) all transient" — and succeeds (`{0}`) even when nothing is
  open. The ABORT form is new in v2: UNAPI defines it (TCPIP_TCP_ABORT,
  B = 0), but v1 silently no-opped it — the device rejected handle 0 and
  the driver never looked at the reply, so the caller got ERR_OK with
  nothing aborted.
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
  **TCP_ABORT** drops immediately (closeReason 3); handle 0 aborts
  every transient connection, mirroring UNAPI's TCPIP_TCP_ABORT.
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
* **UDP_RECV**: the head datagram is consumed whole. `len` in the reply
  is the datagram's size **as received**, and the payload that follows
  is the first `min(len, maxlen)` bytes — the excess, if any, is
  discarded with the datagram. This is exactly TCPIP_UDP_RCV's contract:
  its BC output is "the datagram data size as it was received, which may
  be larger than the number of bytes actually retrieved", and its DE=0
  ("no data will be copied at all") is a *deliberate* discard — so
  `maxlen` 0 is valid: header only, datagram gone. Contrast TCP_RECV,
  where `maxlen` 0 consumes nothing — a stream has no unit to destroy,
  a datagram is all-or-nothing. An empty datagram (0 data bytes) is a
  normal datagram: UDP makes it visible to the receiver, so it queues
  and is delivered with `len` 0. Datagrams were already truncated to
  2 KiB at receive time and queue at most 16 deep (v1 behavior,
  unchanged).
* **UDP_STATE** answers the head datagram's size, which is 0 both for
  an empty queue and for an empty datagram at its head — "UDP_RECV until
  `{3}`" is the idiom that tells them apart, and it is TCPIP_UDP_STATE's
  own advice (its datagram count must not be relied upon).
* **UDP_OPEN**: local port 0xFFFF requests an ephemeral port; local port
  0 reaches the host `bind()` where it also yields an ephemeral port —
  the two coincide by different routes. A requested port the host
  refuses to bind falls back to an ephemeral one too, silently: the
  host OS itself may own the very port the MSX asks for — Windows'
  time service holds UDP 123, which is exactly what an SNTP client
  requests — a collision that cannot exist on the real-hardware UNAPI
  stacks this bridge stands in for, and a client's local port carries
  no meaning for the peer. (The first v2 build answered `{2}` there,
  in the name of honesty; functional testing vetoed it — SNTP broke on
  every Windows host whose time service was running — so v1's silent
  fallback is restored, and this time written down.) `{2}` remains the
  answer when the ephemeral bind fails as well.

### ICMP

* With caps bit4 clear, ICMP_SEND and ICMP_RECV answer `{1}` — v1
  accepted the request, dropped it silently, and let ICMP_RECV report
  "no data" forever; the capability bit becomes enforced, not
  decorative.
* The echo-size field keeps v1's clamp to 512. On a capable host a
  well-formed ICMP_SEND cannot fail: `{0}` acknowledges queueing to the
  worker, not delivery — an unreachable destination simply never yields
  a reply.
* Replies queue FIFO, at most 16 deep; when the queue is full the
  **oldest** reply is dropped — a reply nobody polled for is worth less
  than the fresh one the current program is waiting for (v1 dropped the
  newest; that was code, not design). Each successful ICMP_RECV consumes
  exactly one.
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

`DNS_QUERY` — parameters: the hostname itself. The whole parameter
block is the name; there is no terminator (the block length already
delimits it — v1's NUL was redundant, as Wouter noted). The
device applies no syntax or charset rules to the hostname — beyond the
dotted-quad fast path below, the bytes go to the host resolver as-is.
The two bounds are transport, not syntax: a block longer than
**253 bytes** (the DNS presentation-form name limit) is "too long" under
rule 3 → `{4}`, and a block with an **embedded NUL** is malformed →
`{4}` — the host resolver API speaks C strings and cannot carry the
name past the NUL, so no lookup could ever see it; rejecting the block
beats resolving a silently truncated prefix. A block that
parses as a strict dotted-quad (`a.b.c.d`, four decimal octets 0-255)
resolves immediately → `{0, 1, ip4}`, and arms the sticky Complete state
exactly as an asynchronous success does. Anything else starts the
resolver thread → `{0, 0}`. An empty hostname (an empty block), one
beyond the length cap, or one with an embedded NUL answers `{4}`; a
lookup already running answers `{5}`; all of these leave the DNS state
and any running lookup untouched.

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
dropped, and the DNS state returns to idle. The ICMP capability stays as
latched at device start; reset does not re-probe it.

## Migration and failure analysis

Both halves ship together; the four v1 users get the new `unapinet.com`
with the new openMSX build. The interesting cases are the mismatches:

* **v1 driver, v2 device.** The driver sends opcode 0x00 (its PING) and
  expects the register to read `STATUS_DATA` (2) and the data port to
  yield `0xAB`. On the v2 device the register read hits an unregistered
  port and returns `0xFF`, and the data port yields `0x00 0x55 ...`. Both
  checks fail on the first byte → the driver prints "extension not
  found" and exits. Nothing misbehaves quietly.
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
openMSX and restored on a new one) is not served, but it fails safe:
v1's per-call gate is `status register == 2`, and on a v2 device that
read returns `0xFF` from the unregistered port, so every call reports
failure instead of half-working. Re-running the driver's installer is
the answer; the spec simply does not promise more.

## How the open points resolved

The proposal closed with three points for review; for the record:

1. The magic value (0x55) and the capability bit assignments stood
   unchallenged, and are final.
2. NET_STATE keeps its constant answer. Real semantics — or retirement —
   remain a question for after v2, deliberately not bundled into this
   redesign.
3. The err column and the validation order became the contract the TSR
   was written against, as intended, and the golden-byte tests pin the
   layout of every reply struct (the two-byte replies DNS and NET_STATE
   assemble inline are covered by the spec table alone). The one place
   where reality pushed back is recorded above: UDP_OPEN's ephemeral
   fallback.

---

## Appendix — the v1 protocol (historical)

The v1 document follows — verbatim below its old title, headings demoted
one level — as it stood the day v2 replaced it. It stays authoritative
for the parameter-block layouts, which v2 did not move (DNS_QUERY's
dropped terminator is the one exception), and its closing catalogue of
v1 inconsistencies is the list this redesign set out to fix. Where it
contradicts v2 — past-end reads, the status register, the detection
handshake, the error conventions — everything above supersedes it. Its
code references describe the v1-era sources.

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

### I/O ports

The device claims two consecutive I/O ports, base 0x28 (`share/extensions/
unapinet.xml`, `<io base="0x28" num="2"/>`). Decoding is on bit 0 of the
port number:

| Port | Write            | Read                          |
|------|------------------|-------------------------------|
| 0x28 | command (opcode) | status register               |
| 0x29 | parameter byte   | result byte (consuming)       |

### Transaction model

Every operation is one synchronous transaction:

1. The MSX writes the parameter block, one byte at a time, to port 0x29.
2. The MSX writes the command opcode to port 0x28. The device processes the
   command *inside that I/O write*: by the time the OUT instruction
   completes, the result is ready. The MSX never has to wait or poll for
   command completion. (Operations that take real time - DNS lookups,
   connection establishment, ICMP echo - return immediately and are polled
   with their own dedicated commands; see "Asynchronous operations".)
3. The MSX reads the status register (port 0x28). `0x02` (`STATUS_DATA`)
   means the result is ready to be read.
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

#### Status register values

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

### Detection handshake

The driver detects the device at installation time:

1. `PING` (0x00, no parameters) must answer `STATUS_DATA` with the single
   magic byte **0xAB**. Anything else means the extension is not present.
2. `QUERY_CAP` (0x10, no parameters) answers two bytes: a capability
   summary (0x0F = PING + DNS + TCP + UDP) and the bridge version (4).

*v1 quirk:* the current driver only performs step 1; nothing reads the
version byte today.

### Data types

* **IPv4 address** - 4 bytes, the octets of `a.b.c.d` in order
  (network/big-endian; `Endian::UA_B32` in the structs).
* **port / length / id / seq** - 2 bytes, low byte first (little-endian,
  Z80-natural; `Endian::UA_L16`).
* **handle** - 1 byte. TCP and UDP handles are independent ranges, both
  1-based on the wire: 1..4 (`MAX_TCP` = `MAX_UDP` = 4). In results, handle
  0 means "could not open".

### Command reference

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

#### TCP

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

#### UDP

**UDP_OPEN** - local port (2), 0xFFFF = ephemeral. **UDP_STATE** answers
the size of the first queued datagram (2 bytes; 0 = queue empty, or bad
handle - same v1 quirk as TCP_RECV). **UDP_SEND** - header {handle (1),
destination IP (4), destination port (2), length (2)} + payload; result 1
also covers a short kernel write. **UDP_RECV** - {handle (1), maxlen (2)};
answers {source IP (4), source port (2), actualLen (2)} + payload.
Datagram semantics apply: if the first queued datagram is larger than
`maxlen`, the excess of *that datagram* is discarded.

#### ICMP

**ICMP_SEND** - {destination IP (4), TTL (1), identifier (2),
sequence (2), size (2, clamped to 512)}. No payload follows: the bridge
fabricates the echo data itself. The request is queued to a worker thread;
the result byte only acknowledges the queueing. **ICMP_RECV** polls for
replies: 1 byte 0 = none yet, otherwise {1, source IP (4), TTL (1),
identifier (2), sequence (2), dataLen (2)} - header only, the echo payload
is not returned. *Current limitation:* the ICMP worker is implemented with
the Windows ICMP API; on other host platforms ICMP_RECV never reports
data.

### Asynchronous operations

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

### Known v1 inconsistencies, and the v2 plan

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

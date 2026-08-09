#include "UnapiNet.hh"

#include "MSXException.hh"
#include "one_of.hh"
#include "serialize.hh"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#ifdef interface
// The Microsoft headers above define the 'interface' macro again (Socket.hh
// already undoes the one from winsock2). Undo it here too, so it cannot
// clobber openMSX code that uses the word as an identifier.
#undef interface
#endif
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <errno.h>
#endif

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <expected>

// Socket handles are openMSX SOCKET values (see Socket.hh); no casts needed.

// UnapiNet - MSX-UNAPI TCP/IP bridge device
//
// Bridge between the MSX I/O ports and BSD sockets on the host, speaking
// protocol v2 (see unapinet/protocol-v2.md): every reply begins with a
// status byte carrying an MSX-UNAPI error code verbatim, so the Z80 driver
// hands it to its caller without translating. Async DNS, TCP and UDP (4
// connections each), ICMP echo where the host supports it.

namespace openmsx {

// MSX-UNAPI TCP/IP error codes: v2 speaks them natively on the wire (the
// status byte fronting every reply), so these values are shared with the
// Z80 driver's error table.
static constexpr uint8_t ERR_OK           = 0;
static constexpr uint8_t ERR_NOT_IMP      = 1;
static constexpr uint8_t ERR_NO_NETWORK   = 2;
static constexpr uint8_t ERR_NO_DATA      = 3;
static constexpr uint8_t ERR_INV_PARAM    = 4;
static constexpr uint8_t ERR_QUERY_EXISTS = 5;
static constexpr uint8_t ERR_NO_FREE_CONN = 9;
static constexpr uint8_t ERR_NO_CONN      = 11;
static constexpr uint8_t ERR_CONN_STATE   = 12;
static constexpr uint8_t ERR_BUFFER       = 13;

// DETECT capability byte: bits 0-3 are informational and always set on this
// device; bit4 (ICMP) is honest - latched once at device start and enforced
// by the ICMP commands.
static constexpr uint8_t CAPS_BASE = 0x0F; // DNS + TCP active/passive + UDP
static constexpr uint8_t CAP_ICMP  = 0x10;

// Maximum result payload per TCP_RECV command, and the exact-size ceiling
// on a TCP_SEND length (rule 3 makes exceeding it ERR_INV_PARAM).
static constexpr size_t MAX_TRANSFER = 4096;

// Maximum receive buffer size per connection
// BBS ANSI screens can be 16-32KB, needs big buffer
static constexpr size_t MAX_RECV_BUF = 65536;

// Upper bound on accumulated command parameters: one byte MORE than the
// largest legal parameter block (UDP_SEND: 9-byte header + a full 16-bit
// payload). A block the cap truncated therefore always ends on an illegal
// size, so every command's size check rejects it - no separate overflow
// flag needed - while a runaway MSX program hammering the data port still
// cannot exhaust host memory.
static constexpr size_t MAX_PARAM_BUF = sizeof(UdpSendParamHeader) + 0xFFFF + 1;

// DNS_QUERY's own size cap (rule 3 "too long"): the DNS presentation-form
// name limit (RFC 1035). Keeps the one variable-length command that has no
// declared payload length from depending on MAX_PARAM_BUF for rejection.
static constexpr size_t MAX_HOSTNAME = 253;

// Bound on data queued for sending but not yet accepted by the kernel.
// TCP_SEND reports ERR_BUFFER rather than blocking the emulation thread.
static constexpr size_t MAX_SEND_BUF = 64 * 1024;

// Bounds on the receive-side queues (v1 values, unchanged in v2).
static constexpr size_t MAX_UDP_QUEUE  = 16;   // pending datagrams per socket
static constexpr size_t MAX_ICMP_QUEUE = 16;   // pending echo replies
static constexpr size_t MAX_UDP_DGRAM  = 2048; // larger datagrams are truncated at receive

// Bridge command opcodes (wire protocol, shared with the Z80 driver).
// 0x10 (v1's QUERY_CAP) is retired: DETECT carries the capabilities, and
// the opcode answers ERR_NOT_IMP like any other unknown one.
static constexpr uint8_t CMD_DETECT      = 0x00;
static constexpr uint8_t CMD_DNS_QUERY   = 0x01;
static constexpr uint8_t CMD_DNS_STATUS  = 0x02;
static constexpr uint8_t CMD_TCP_OPEN    = 0x03;
static constexpr uint8_t CMD_TCP_SEND    = 0x04;
static constexpr uint8_t CMD_TCP_RECV    = 0x05;
static constexpr uint8_t CMD_TCP_CLOSE   = 0x06;
static constexpr uint8_t CMD_TCP_STATE   = 0x07;
static constexpr uint8_t CMD_TCP_ABORT   = 0x08;
static constexpr uint8_t CMD_UDP_OPEN    = 0x09;
static constexpr uint8_t CMD_UDP_CLOSE   = 0x0A;
static constexpr uint8_t CMD_UDP_STATE   = 0x0B;
static constexpr uint8_t CMD_UDP_SEND    = 0x0C;
static constexpr uint8_t CMD_GET_LOCALIP = 0x0D;
static constexpr uint8_t CMD_NET_STATE   = 0x0E;
static constexpr uint8_t CMD_UDP_RECV    = 0x0F;
static constexpr uint8_t CMD_ICMP_SEND   = 0x11;
static constexpr uint8_t CMD_ICMP_RECV   = 0x12;

// How long a half-closed connection may wait for the peer to close too,
// before we drop it and free the handle.
static constexpr auto CLOSE_TIMEOUT = std::chrono::seconds(30);

// openMSX's sock_recv()/sock_send() fold 'peer closed' and 'error' into -1,
// and decide would-block from SO_ERROR - which a synchronous WSAEWOULDBLOCK
// does not set on Windows. We need the outcomes apart, so we call
// recv()/send() here and ask the platform ourselves.
enum class IoError { WouldBlock, Closed, Failed };
using IoResult = std::expected<size_t, IoError>; // success: bytes transferred

// 'try again later' rather than a broken connection.
[[nodiscard]] static bool ioWouldBlock()
{
#ifdef _WIN32
	int err = WSAGetLastError();
	return (err == WSAEWOULDBLOCK) || (err == WSAEINTR);
#else
	return (errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINTR);
#endif
}

[[nodiscard]] static IoResult netRecv(SOCKET sd, char* buf, size_t count)
{
	auto n = recv(sd, buf, static_cast<int>(count), 0);
	if (n > 0) return static_cast<size_t>(n);
	if (n == 0) return std::unexpected(IoError::Closed); // orderly shutdown by the peer
	return std::unexpected(ioWouldBlock() ? IoError::WouldBlock : IoError::Failed);
}

[[nodiscard]] static IoResult netSend(SOCKET sd, const uint8_t* buf, size_t count)
{
	// MSG_NOSIGNAL: writing to a connection the peer has reset raises SIGPIPE
	// otherwise, and nothing in openMSX ignores that signal - it would take
	// the whole emulator down.
#ifdef MSG_NOSIGNAL
	constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
	constexpr int SEND_FLAGS = 0; // Windows has no SIGPIPE
#endif
	auto n = send(sd, reinterpret_cast<const char*>(buf),
	              static_cast<int>(count), SEND_FLAGS);
	if (n >= 0) return static_cast<size_t>(n);
	return std::unexpected(ioWouldBlock() ? IoError::WouldBlock : IoError::Failed);
}

// Half-close: tell the peer we are done sending (FIN).
static void shutdownSend(SOCKET sd)
{
#ifdef _WIN32
	shutdown(sd, SD_SEND);
#else
	shutdown(sd, SHUT_WR);
#endif
}

// UNAPI DNS_S sub-error reported on a failed lookup (host name does not exist)
static constexpr uint8_t DNS_ERR_NO_SUCH_HOST = 3;

// Constructor / Destructor

UnapiNet::UnapiNet(const DeviceConfig& config)
	: MSXDevice(config)
{
	paramBuf.reserve(MAX_TRANSFER + 16);
	resultBuf.reserve(MAX_TRANSFER + 16);

	reset(EmuTime::dummy()); // keep constructor and reset() in sync

	// The loopback wake pair (see the header). Two UDP sockets on
	// 127.0.0.1: if any step fails, the host's network stack is broken
	// and every socket this device would later hand out is doomed the
	// same way - so refuse to construct instead of running degraded.
	// Requiring the pair makes poke() a guaranteed wake-up and leaves
	// the socket loop a single code path, the one that gets tested.
	// (Created before the ICMP handle: if this throws, the destructor
	// never runs, so nothing needing cleanup may precede it.)
	SOCKET r = socket(AF_INET, SOCK_DGRAM, 0);
	if (r == OPENMSX_INVALID_SOCKET) {
		throw MSXException("UnapiNet: cannot create the wake socket pair");
	}
	sockaddr_in addr = sock_makeIPv4(INADDR_LOOPBACK, 0); // any free port
	::socklen_t alen = sizeof(addr);
	if (bind(r, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
	    getsockname(r, reinterpret_cast<sockaddr*>(&addr), &alen) != 0) {
		sock_close(r);
		throw MSXException("UnapiNet: cannot bind the wake socket pair");
	}
	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == OPENMSX_INVALID_SOCKET ||
	    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
		if (s != OPENMSX_INVALID_SOCKET) sock_close(s);
		sock_close(r);
		throw MSXException("UnapiNet: cannot connect the wake socket pair");
	}
	sock_setNonBlocking(r);
	sock_setNonBlocking(s);
	wakeRecv = r;
	wakeSend = s;

#ifdef _WIN32
	// Latch the ICMP capability, once: DETECT advertises bit4 only when
	// pinging can actually work, and the ICMP commands are gated on it.
	// (IcmpSendEcho needs no privileges, unlike raw sockets.)
	if (HANDLE h = IcmpCreateFile(); h != INVALID_HANDLE_VALUE) {
		icmpChannel = h;
		icmpAvailable = true;
	}
#endif

	// Start background threads
	running = true;
	sockThread = std::thread([this]() { socketLoop(); });
	dnsThread  = std::thread([this]() { dnsWorkerLoop(); });
	if (icmpAvailable) {
		icmpWorker = std::thread([this]() { icmpWorkerLoop(); });
	}
}

UnapiNet::~UnapiNet()
{
	running = false;
	poke(); // leave select() now instead of at the next timeout
	if (sockThread.joinable()) sockThread.join();
	// Wake the ICMP worker the same way as the DNS worker below (the
	// empty critical section is explained there). If it is inside
	// IcmpSendEcho() the join waits the call out (capped at 2 s).
	{
		std::scoped_lock lock(icmpMutex);
	}
	icmpCv.notify_all();
	if (icmpWorker.joinable()) icmpWorker.join();
	// Wake the DNS worker so it observes the shutdown. If it is inside
	// getaddrinfo() the join waits that call out - deliberate, so the
	// resolver cannot write into a destroyed object.
	//
	// The empty critical section is load-bearing, not a refactoring
	// leftover: it closes the lost-wakeup window. The worker evaluates its
	// wait predicate under this mutex and cv.wait() releases-and-parks
	// atomically, so taking the mutex once *after* flipping 'running'
	// guarantees the worker is either before the predicate (it will see
	// running == false) or already parked (it will get the notify below).
	// Without it, the notify could fire in the gap between the worker
	// checking 'running' and parking - and the join would wait forever.
	{
		std::scoped_lock lock(dns.mutex);
	}
	dns.cv.notify_all();
	if (dnsThread.joinable())  dnsThread.join();
	closeAllConnections();
	// The socket thread stopped without draining its queue: close what is left,
	// or we leak every fd handed over in the last select cycle.
	{
		std::scoped_lock lock(closeMutex);
		for (SOCKET sd : socksToClose) sock_close(sd);
		socksToClose.clear();
	}
	sock_close(wakeSend);
	sock_close(wakeRecv);

#ifdef _WIN32
	if (icmpChannel) IcmpCloseHandle(icmpChannel);
#endif
}

// Reset

void UnapiNet::reset(EmuTime /*time*/)
{
	// Ground state: every connection closed and freed, every buffer and
	// queue discarded, pending reply and accumulated parameters dropped,
	// and DNS idle. The ICMP capability stays as latched at device start.
	paramBuf.clear();
	resultBuf.clear();
	resultPos = 0;

	// Ask the socket thread to drop everything (it owns the sock_close()).
	// During construction the thread doesn't exist yet - but neither do any
	// sockets, so this is a no-op then.
	for (auto& c : tcp) requestClose(c, CloseReason::NeverUsed, true);
	for (auto& u : udp) requestClose(u);

	{
		std::scoped_lock lock(dns.mutex);
		dns.status = DnsStatus::Idle;
		dns.resolvedIp = 0;
		dns.request.reset(); // disowns a lookup still in flight (it only
		                     // publishes while 'request' holds its name)
		                     // and discards a queued one in the same move
	}
	{
		std::scoped_lock lock(icmpMutex);
		icmpReplies.clear();
		++icmpGeneration; // an echo still in flight must not repopulate
		                  // the queue we just cleared
		icmpPending = false; // and a queued-but-unstarted one never starts
	}
}

// Port reads

byte UnapiNet::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	// Only the data register (typically 0x29) is registered for reads; the
	// command port is write-only in v2, so a read there decodes to the
	// open bus without reaching us. This returns the next unread reply
	// byte. Past the end of a reply, or with no reply pending, reads 0xFF -
	// the open-bus value an absent device yields, and never a valid
	// status (rule 4): a desynchronized driver can always tell.
	if (resultPos < resultBuf.size()) {
		return resultBuf[resultPos];
	}
	return 0xFF;
}

byte UnapiNet::readIO(uint16_t port, EmuTime time)
{
	byte b = peekIO(port, time);
	// reading the data register consumes one result byte
	if (resultPos < resultBuf.size()) {
		++resultPos;
	}
	return b;
}

// Port writes

void UnapiNet::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	if (port & 1) {
		// parameter byte (typically 0x29). Writing one while a reply is
		// pending abandons the reply (the recovery rule) - the driver never
		// has to drain a result it lost interest in.
		resultBuf.clear();
		resultPos = 0;
		if (paramBuf.size() < MAX_PARAM_BUF) {
			paramBuf.push_back(value);
		}
		// else: drop the byte. The cap is one past the largest legal
		// block, so a truncated block is already "too long" (rule 3) at
		// every command's size check - dropping loses nothing.
	} else {
		// command (typically 0x28); replaces a pending reply, if any
		processCmd(value);
	}
}

// Result helpers

void UnapiNet::setResult(std::span<const uint8_t> data)
{
	assert(!data.empty()); // v2: every reply begins with its status byte
	resultBuf.assign(data.begin(), data.end());
	resultPos = 0;
}

void UnapiNet::replyStatus(uint8_t status)
{
	setResult(std::span<const uint8_t>(&status, 1));
}

// TCP handle management

int UnapiNet::allocTcpHandle()
{
	// A closed slot holding the sticky tail of a dead connection IS free:
	// it does not count toward ERR_NO_FREE_CONN, and reuse destroys the tail
	// (cmdTcpOpen clears the buffers when it publishes).
	for (int i = 0; i < MAX_TCP; i++) {
		if (tcp[i].sock == OPENMSX_INVALID_SOCKET &&
			tcp[i].tcpState == TcpState::Closed) {
			return i;
		}
	}
	return INVALID_HANDLE;
}

// The wire handle is a byte from the MSX: 1-based, 1..4 in range. These two
// are the only places that map wire handles to the 0-based internal arrays.
UnapiNet::TcpConnection* UnapiNet::tcpForHandle(int wireHandle)
{
	return (wireHandle >= 1 && wireHandle <= MAX_TCP) ? &tcp[wireHandle - 1] : nullptr;
}

UnapiNet::UdpConnection* UnapiNet::udpForHandle(int wireHandle)
{
	return (wireHandle >= 1 && wireHandle <= MAX_UDP) ? &udp[wireHandle - 1] : nullptr;
}

// Direct close. Only safe with the socket thread stopped (destructor).
void UnapiNet::closeTcp(TcpConnection& c)
{
	std::scoped_lock lock(c.mutex);
	if (SOCKET sd = c.sock; sd != OPENMSX_INVALID_SOCKET) {
		sock_close(sd);
		c.sock = OPENMSX_INVALID_SOCKET;
	}
	c.finSent = false;
	c.tcpState   = TcpState::Closed;
	c.remoteIp   = 0;
	c.remotePort = 0;
	c.localPort  = 0;
	c.resident   = false;
	c.recvBuf.clear();
	c.sendBuf.clear();
}

void UnapiNet::closeAllConnections()
{
	for (auto& c : tcp) {
		closeTcp(c);
		c.closeReason = CloseReason::NeverUsed;
	}
	for (auto& u : udp) {
		closeUdp(u);
	}
}

// Network socket thread (background)
//
// One select() loop serves every socket: it moves incoming TCP data into
// each connection's recvBuf, flushes queued sends, detects completion of
// a non-blocking connect() and state transitions (remote close, etc.),
// receives UDP datagrams, and performs every close. It sleeps in
// select() until traffic arrives or the emulation thread pokes the wake
// socket at it.

// Wake the socket thread out of select(): one byte to the loopback
// socket it always watches (the constructor guarantees the pair
// exists). The result is deliberately dropped: a refused send can only
// mean the wake socket already holds unread pokes, so select() has a
// byte to wake on either way - the poke is a hint, never a message,
// and the loop re-derives all its work from shared state.
void UnapiNet::poke()
{
	uint8_t b = 0;
	[[maybe_unused]] auto result = netSend(wakeSend, &b, 1);
}

void UnapiNet::deferSockClose(SOCKET sd)
{
	std::scoped_lock lock(closeMutex);
	socksToClose.push_back(sd);
	poke(); // the fd is closed at the top of the loop's next pass
}

void UnapiNet::requestClose(TcpConnection& c, CloseReason reason,
                            bool clearMetadata)
{
	SOCKET sd;
	{
		std::scoped_lock lock(c.mutex);
		sd = c.sock;
		c.sock = OPENMSX_INVALID_SOCKET; // the handle is reusable right away
		c.closeReason = reason;
		c.tcpState = TcpState::Closed;
		c.finSent = false;
		c.sendBuf.clear();
		if (clearMetadata) { // reset() only: the sticky-slot rule keeps
			c.remoteIp   = 0;  // the endpoint info and the undrained tail
			c.remotePort = 0;  // readable everywhere else
			c.localPort  = 0;
			c.resident   = false;
			c.recvBuf.clear();
		}
	}
	if (sd != OPENMSX_INVALID_SOCKET) deferSockClose(sd);
}

void UnapiNet::requestClose(UdpConnection& u)
{
	// Unlike TCP there is no sticky tail: closing a UDP socket discards
	// its queued datagrams.
	SOCKET sd;
	{
		std::scoped_lock lock(u.mutex);
		sd = u.sock;
		u.sock = OPENMSX_INVALID_SOCKET;
		u.localPort = 0;
		u.resident  = false;
		u.recvQueue.clear();
	}
	if (sd != OPENMSX_INVALID_SOCKET) deferSockClose(sd);
}

// TCP_CLOSE. A listening or still-connecting socket has nothing to
// half-close, so it is simply dropped. An established one moves to
// FinWait1: the FIN goes out as soon as whatever the MSX queued has been
// sent, and the connection is dropped when the peer closes too (or when
// CLOSE_TIMEOUT passes and it never does). A second CLOSE while that is
// in progress changes nothing (idempotent).
void UnapiNet::gracefulClose(TcpConnection& c)
{
	{
		// Test and act under one lock: the socket thread may have dropped this
		// connection (peer reset) between the two, which would leave FinWait1
		// stamped on a slot that has no socket - unusable and unrecoverable.
		std::scoped_lock lock(c.mutex);
		SOCKET sd = c.sock;
		if (sd != OPENMSX_INVALID_SOCKET) {
			if (c.tcpState == TcpState::FinWait1) {
				return; // close already in progress: idempotent
			}
			if (c.tcpState == one_of(TcpState::Established, TcpState::CloseWait)) {
				c.closeReason = CloseReason::ClosedByUser;
				c.tcpState = TcpState::FinWait1;
				c.closeDeadline = std::chrono::steady_clock::now() + CLOSE_TIMEOUT;
				c.finSent = false;
				if (c.sendBuf.empty()) {
					shutdownSend(sd);
					c.finSent = true;
				}
				poke(); // the loop now tracks this close (FIN flush,
				        // peer's close, deadline)
				return;
			}
		}
	} // drop the lock: requestClose() takes it
	// Nothing to half-close (listening, still connecting, or already gone).
	requestClose(c, CloseReason::ClosedByUser);
}

void UnapiNet::socketLoop()
{
	while (running) {
		// Close whatever the emulation thread handed over. Doing it here, at the
		// top of a pass, means no select() is in flight on those fds, so their
		// numbers cannot be recycled under a thread that is still watching them.
		{
			std::vector<SOCKET> toClose;
			{
				std::scoped_lock lock(closeMutex);
				toClose.swap(socksToClose);
			}
			for (SOCKET sd : toClose) sock_close(sd);
		}

		// Wait on all active sockets at once with a single select(),
		// instead of busy-polling each socket in turn. The wake socket is
		// always in the read set, so new work from the emulation thread
		// interrupts the wait immediately; the timeout is a backstop - it
		// paces the FinWait1 deadline check.
		fd_set rfds;
		fd_set wfds;
		fd_set efds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);
		// The wake socket goes into every read set (the constructor
		// guarantees the pair exists), so select() always has at least
		// one fd to watch - there is no "empty set" case to special-case.
		FD_SET(wakeRecv, &rfds);
		SOCKET maxSock = wakeRecv;
		// Remember exactly which fd we armed for each slot: after select() the
		// emulation thread may have closed and reopened one, and a stale bit in
		// the fd sets must not be applied to the new socket.
		std::array<SOCKET, MAX_TCP> watchedTcp;
		std::array<SOCKET, MAX_UDP> watchedUdp;
		watchedTcp.fill(OPENMSX_INVALID_SOCKET);
		watchedUdp.fill(OPENMSX_INVALID_SOCKET);

		for (int i = 0; i < MAX_TCP; ++i) {
			auto& c = tcp[i];
			SOCKET sd = c.sock;
			if (sd == OPENMSX_INVALID_SOCKET) continue;

			// Half-close in progress (the MSX called TCP_CLOSE).
			if (c.tcpState == TcpState::FinWait1) {
				bool giveUp = false;
				{
					std::scoped_lock lock(c.mutex);
					if (!c.finSent && c.sendBuf.empty()) {
						// Everything the MSX queued is out: now it is safe to FIN.
						shutdownSend(sd);
						c.finSent = true;
					}
					giveUp = std::chrono::steady_clock::now() > c.closeDeadline;
				}
				if (giveUp) { // the peer never closed its side
					requestClose(c, CloseReason::ClosedByUser);
					continue;
				}
			}

			watchedTcp[i] = sd;
			if (c.tcpState == TcpState::SynSent) {
				// connect() completion shows up as writable (POSIX) or in
				// the exception set (Windows).
				FD_SET(sd, &wfds);
				FD_SET(sd, &efds);
			} else {
				// Only ask for incoming data while recvBuf has room for it.
				// Leaving a full socket unread lets the kernel receive buffer
				// fill up, which closes the TCP window and makes the peer stop
				// sending - that is the flow control TCP already provides.
				// recv()ing anyway and dropping whatever doesn't fit would
				// silently truncate the stream instead.
				// While recvBuf is full we don't notice the peer closing its
				// side either; that is picked up as soon as the MSX drains
				// some bytes and the socket is armed again.
				bool pendingSend;
				bool hasRoom;
				{
					std::scoped_lock lock(c.mutex);
					pendingSend = !c.sendBuf.empty();
					hasRoom = c.recvBuf.size() < MAX_RECV_BUF;
				}
				if (hasRoom)     FD_SET(sd, &rfds);
				if (pendingSend) FD_SET(sd, &wfds);
			}
			maxSock = std::max(maxSock, sd);
		}
		for (int i = 0; i < MAX_UDP; ++i) {
			auto& u = udp[i];
			SOCKET sd = u.sock;
			if (sd == OPENMSX_INVALID_SOCKET) continue;
			watchedUdp[i] = sd;
			FD_SET(sd, &rfds);
			maxSock = std::max(maxSock, sd);
		}
		struct timeval tv = {0, 100000}; // 100 ms
		if (select(static_cast<int>(maxSock) + 1, &rfds, &wfds, &efds, &tv) <= 0) {
			continue; // timeout or error: re-check running and rebuild the set
		}
		if (FD_ISSET(wakeRecv, &rfds)) {
			// Drain every queued poke. Each one is only a hint to rescan;
			// all the actual work is re-derived from shared state below.
			std::array<char, 64> pokes;
			while (netRecv(wakeRecv, pokes.data(), pokes.size())) {}
		}

		for (int i = 0; i < MAX_TCP; ++i) {
			auto& c = tcp[i];
			SOCKET sd = watchedTcp[i];
			// Skip slots we did not arm, and slots whose socket changed while we
			// were in select(): the fd sets refer to the old one.
			if (sd == OPENMSX_INVALID_SOCKET || c.sock != sd) continue;

			// Pending connect() completion.
			if (c.tcpState == TcpState::SynSent) {
				if (FD_ISSET(sd, &efds)) {
					requestClose(c, CloseReason::ConnectFailed);
				} else if (FD_ISSET(sd, &wfds)) {
					int err = sock_getIntOption(sd, SOL_SOCKET, SO_ERROR);
					if (err != 0) {
						requestClose(c, CloseReason::ConnectFailed);
					} else {
						std::scoped_lock lock(c.mutex);
						if (c.sock == sd) c.tcpState = TcpState::Established;
					}
				}
				continue;
			}

			// Listening socket: accept the pending connection.
			if (c.tcpState == TcpState::Listen) {
				if (!FD_ISSET(sd, &rfds)) continue;
				struct sockaddr_in peer;
				::socklen_t plen = sizeof(peer);
				SOCKET a = accept(sd, reinterpret_cast<struct sockaddr*>(&peer), &plen);
				if (a == OPENMSX_INVALID_SOCKET) continue;
				uint32_t peerIp = ntohl(peer.sin_addr.s_addr);
				uint16_t peerPort = ntohs(peer.sin_port);
				sock_setNonBlocking(a);
				sock_setIntOption(a, IPPROTO_TCP, TCP_NODELAY);

				bool taken = false;
				{
					std::scoped_lock lock(c.mutex);
					// The MSX may have closed the listener while we were accepting,
					// and the remote-IP filter has to be read coherently. A
					// non-matching peer is dropped and the listener keeps
					// listening; on a match the handle BECOMES the connection.
					if (c.sock == sd && c.tcpState == TcpState::Listen &&
					    (c.remoteIp == 0 || peerIp == c.remoteIp)) {
						// Swap the listening socket for the accepted one, publishing
						// the state and the address together.
						c.sock       = a;
						c.tcpState   = TcpState::Established;
						c.remoteIp   = peerIp;
						c.remotePort = peerPort;
						taken = true;
					}
				}
				if (taken) {
					deferSockClose(sd); // the old listener; closed next pass
				} else {
					sock_close(a);
				}
				continue;
			}

			if (c.tcpState != one_of(TcpState::Established, TcpState::CloseWait,
			                         TcpState::FinWait1)) {
				continue;
			}

			// Push out whatever the MSX queued via TCP_SEND. send() on a
			// non-blocking socket cannot block, so doing it under the lock is
			// fine; a full kernel buffer just means 'try again on the next pass'.
			if (FD_ISSET(sd, &wfds)) {
				bool sendFailed = false;
				{
					std::scoped_lock lock(c.mutex);
					while (c.sock == sd && !c.sendBuf.empty()) {
						auto r = netSend(sd, c.sendBuf.data(), c.sendBuf.size());
						if (!r) {
							sendFailed = (r.error() == IoError::Failed);
							break;
						}
						if (*r == 0) break;
						c.sendBuf.erase(c.sendBuf.begin(),
						                c.sendBuf.begin() + *r);
					}
				}
				if (sendFailed) {
					requestClose(c, CloseReason::ConnectionReset);
					continue;
				}
			}

			// Incoming data. Never ask the kernel for more than fits: whatever
			// we take out of its buffer and cannot store would be lost, and the
			// peer would never know. Reading only 'room' bytes leaves the rest
			// in the kernel, where it keeps the TCP window closed until the MSX
			// makes space.
			if (!FD_ISSET(sd, &rfds)) continue;
			size_t room;
			{
				std::scoped_lock lock(c.mutex);
				if (c.sock != sd) continue;
				room = MAX_RECV_BUF - std::min(MAX_RECV_BUF, c.recvBuf.size());
			}
			if (room == 0) continue; // full: we shouldn't even have armed it
			std::array<char, 512> buf;
			auto r = netRecv(sd, buf.data(), std::min(buf.size(), room));
			if (r) {
				std::scoped_lock lock(c.mutex);
				if (c.sock != sd) continue;
				const auto* d = reinterpret_cast<const uint8_t*>(buf.data());
				c.recvBuf.insert(c.recvBuf.end(), d, d + *r);
			} else if (r.error() == IoError::Closed) {
				// The peer closed its side. If we had closed ours too, the
				// connection is finished; otherwise the MSX may still send.
				if (c.tcpState == TcpState::FinWait1) {
					requestClose(c, CloseReason::ClosedByUser);
				} else {
					std::scoped_lock lock(c.mutex);
					if (c.sock == sd) c.tcpState = TcpState::CloseWait;
				}
			} else if (r.error() == IoError::Failed) {
				requestClose(c, CloseReason::ConnectionReset);
			}
			// WouldBlock: spurious readable, nothing to do
		}

		for (int i = 0; i < MAX_UDP; ++i) {
			auto& u = udp[i];
			SOCKET sd = watchedUdp[i];
			if (sd == OPENMSX_INVALID_SOCKET || u.sock != sd) continue;
			if (!FD_ISSET(sd, &rfds)) continue;
			std::array<char, MAX_UDP_DGRAM> buf;
			struct sockaddr_in src;
			::socklen_t slen = sizeof(src);
			int n = recvfrom(sd, buf.data(), buf.size(), 0,
			                 reinterpret_cast<struct sockaddr*>(&src), &slen);
#ifdef _WIN32
			// Winsock reports a datagram larger than the buffer as an
			// ERROR (WSAEMSGSIZE) after filling the buffer and consuming
			// the datagram - that IS the truncate-at-receive the protocol
			// wants, not a failure. Treating it as one would silently lose
			// the whole datagram. (POSIX recvfrom just returns a full
			// buffer, no special case needed.)
			if (n < 0 && WSAGetLastError() == WSAEMSGSIZE) {
				n = static_cast<int>(buf.size());
			}
#endif
			// recvfrom() == 0 is not EOF here: an empty UDP datagram is a
			// real datagram (visible to the receiver, unlike a 0-byte TCP
			// send) and queues like any other.
			if (n < 0) continue;
			UdpDatagram dg;
			dg.srcIp = ntohl(src.sin_addr.s_addr);
			dg.srcPort = ntohs(src.sin_port);
			dg.data.assign(buf.data(), buf.data() + n);
			std::scoped_lock lock(u.mutex);
			if (u.sock != sd) continue;
			if (u.recvQueue.size() < MAX_UDP_QUEUE) { // cap pending datagrams
				u.recvQueue.push_back(std::move(dg));
			}
		}
	}
}

// Command processing
//
// Validation order is fixed by the spec: form first (rule 3 plus the
// semantic ERR_INV_PARAMs), then capability, then handle, then connection
// state, then the data check; the host operation runs last, so its errors
// imply every earlier check passed.

void UnapiNet::processCmd(uint8_t cmd)
{
	// Opcode dispatch precedes rule 3: an unknown opcode answers ERR_NOT_IMP
	// whether or not stray parameter bytes accompany it (there is no
	// expected size to validate against).
	switch (cmd) {
	case CMD_DETECT:     cmdDetect();    break;
	case CMD_DNS_QUERY:  cmdDnsQuery();  break;
	case CMD_DNS_STATUS: cmdDnsStatus(); break;
	case CMD_TCP_OPEN:   cmdTcpOpen();   break;
	case CMD_TCP_SEND:   cmdTcpSend();   break;
	case CMD_TCP_RECV:   cmdTcpRecv();   break;
	case CMD_TCP_CLOSE:  cmdTcpClose();  break;
	case CMD_TCP_STATE:  cmdTcpState();  break;
	case CMD_TCP_ABORT:  cmdTcpAbort();  break;
	case CMD_GET_LOCALIP: cmdGetLocalIP(); break;
	case CMD_NET_STATE:  cmdNetState();  break;
	case CMD_UDP_OPEN:   cmdUdpOpen();   break;
	case CMD_UDP_CLOSE:  cmdUdpClose();  break;
	case CMD_UDP_STATE:  cmdUdpState();  break;
	case CMD_UDP_SEND:   cmdUdpSend();   break;
	case CMD_UDP_RECV:   cmdUdpRecv();   break;
	case CMD_ICMP_SEND:  cmdIcmpSend();  break;
	case CMD_ICMP_RECV:  cmdIcmpRecv();  break;
	default:
		replyStatus(ERR_NOT_IMP);
		break;
	}
	paramBuf.clear(); // always clear params after a command
}

// DETECT (0x00)
// Params: none (strictly - rule 3)
// Reply: {0, 0x55, 2, caps, 0}

void UnapiNet::cmdDetect()
{
	// Strict like every other command: the driver issues DETECT twice
	// unconditionally, so a stray parameter block from a crashed
	// predecessor fails the first attempt, is cleared by it, and cannot
	// fail the second.
	if (!paramBuf.empty()) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	setResult(DetectResult{.caps =
		static_cast<uint8_t>(CAPS_BASE | (icmpAvailable ? CAP_ICMP : 0))});
}

// DNS_QUERY (0x01)
// Params: the hostname itself - the block length delimits it, no terminator
// Reply: {0, 0} lookup started, {0, 1, ip4} resolved immediately

void UnapiNet::cmdDnsQuery()
{
	// Form errors first, before the busy check; none of them touches the
	// DNS state or a running lookup. An empty block is an empty hostname,
	// and a block beyond the DNS name limit is "too long" (rule 3) - no
	// real name is that long, and resolving a truncated one would look
	// like success on the wrong name. An embedded NUL is malformed for a
	// kindred reason: the host resolver API speaks C strings and cannot
	// even carry the name past the NUL, so no lookup could ever see it -
	// rejecting it beats resolving a silently truncated prefix.
	if (paramBuf.empty() || paramBuf.size() > MAX_HOSTNAME ||
	    std::ranges::find(paramBuf, uint8_t(0)) != paramBuf.end()) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	{
		std::scoped_lock lock(dns.mutex);
		if (dns.status == DnsStatus::InProgress) {
			// The running lookup owns the DNS state.
			replyStatus(ERR_QUERY_EXISTS);
			return;
		}
	}

	std::string hostname(paramBuf.begin(), paramBuf.end());

	// Dotted-quad fast path: a strict a.b.c.d (four decimal octets) resolves
	// immediately and arms the sticky Complete state exactly as an
	// asynchronous success does. inet_pton is exactly that strict form.
	struct in_addr addr;
	if (inet_pton(AF_INET, hostname.c_str(), &addr) == 1) {
		uint32_t ip = ntohl(addr.s_addr);
		{
			std::scoped_lock lock(dns.mutex);
			dns.resolvedIp = ip;
			dns.status = DnsStatus::Complete;
		}
		setResult(DnsQueryResult{.ip = ip});
		return;
	}

	// Asynchronous resolution. Anything that is not a dotted quad goes to
	// the host resolver as-is: the device applies no syntax rules. The
	// lookup is handed to the persistent worker; the emulation thread
	// never waits on the resolver.
	{
		std::scoped_lock lock(dns.mutex);
		dns.status = DnsStatus::InProgress;
		dns.resolvedIp = 0;
		dns.request = std::move(hostname);
	}
	dns.cv.notify_one();

	const std::array<uint8_t, 2> started{ERR_OK, 0};
	setResult(started);
}

// The persistent DNS worker. It sleeps on the condition variable until
// DNS_QUERY queues a hostname (or the device shuts down), resolves it with
// the mutex released, and publishes the outcome only if dns.request still
// holds the name it resolved - the queued request doubles as the lookup's
// ownership token (Wouter's round-8 simplification). reset() clears the
// token, so a disowned lookup finishes into silence. The one soft spot is
// deliberate: re-querying the *same* name across a reset can be answered
// by the pre-reset lookup - it resolved the same string, so the answer is
// the answer.
void UnapiNet::dnsWorkerLoop()
{
	std::unique_lock lock(dns.mutex);
	while (true) {
		dns.cv.wait(lock, [&] { return !running || dns.request.has_value(); });
		if (!running) return;
		std::string hostname = *dns.request; // copy: 'request' stays engaged
		                                     // as the ownership token
		lock.unlock();

		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		struct addrinfo* res = nullptr;
		int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
		std::optional<uint32_t> ip;
		if (err == 0 && res != nullptr) {
			auto* addr4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
			// Host byte order here; the UA_B32 wire field re-emits it as
			// big-endian octets.
			ip = ntohl(addr4->sin_addr.s_addr);
		}
		if (res) freeaddrinfo(res);

		lock.lock();
		if (dns.request == hostname) { // else: disowned by a reset (and
			                           // possibly superseded), stay silent
			dns.request.reset();
			if (ip) {
				dns.resolvedIp = *ip;
				dns.status = DnsStatus::Complete;
			} else {
				dns.status = DnsStatus::Error;
			}
		}
	}
}

// DNS_STATUS (0x02)
// Params: none
// Reply: {0, 0} idle / {0, 1} in progress / {0, 2, ip4} complete /
//        {0, 0xFF, sub} failed - data, not a command error: the state is
//        sticky until the next DNS_QUERY or a reset

void UnapiNet::cmdDnsStatus()
{
	if (!paramBuf.empty()) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	std::scoped_lock lock(dns.mutex);
	switch (dns.status) {
	case DnsStatus::Complete:
		setResult(DnsStatusResult{.ip = dns.resolvedIp});
		break;
	case DnsStatus::Error:
		// A success reply carrying bad news: the lookup failed, the command
		// did not. Today the only sub-error the resolver reports is 'no
		// such host'.
		setResult(DnsStatusFailed{.sub = DNS_ERR_NO_SUCH_HOST});
		break;
	default: { // Idle (0) / InProgress (1)
		const std::array<uint8_t, 2> r{ERR_OK, static_cast<uint8_t>(dns.status)};
		setResult(r);
		break;
	}
	}
}

// TCP_OPEN (0x03)
// Params: IP[4] + remote_port[2 LE] + local_port[2 LE] + timeout[2] + flags[1]
// Flags bit 0: passive mode (listen). bit 1: resident. bits 2-7: must be 0.
// Reply: {0, handle}

void UnapiNet::cmdTcpOpen()
{
	auto p = exactParams<TcpOpenParams>();
	if (!p) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	if (p->flags & ~0x03) { // undefined flag bits must be 0
		replyStatus(ERR_INV_PARAM);
		return;
	}
	bool passive  = (p->flags & 0x01) != 0;
	bool resident = (p->flags & 0x02) != 0;
	uint32_t ip           = p->remoteIp;
	uint16_t remotePort   = p->remotePort;
	uint16_t localPortReq = p->localPort;
	// p->timeout is accepted with any value and ignored (reserved)

	// The free-slot check precedes the host socket work; a slot consumed by
	// a failure below is never published, i.e. released - ConnectFailed is
	// for the asynchronous connect path only.
	int h = allocTcpHandle();
	if (h == INVALID_HANDLE) {
		replyStatus(ERR_NO_FREE_CONN);
		return;
	}
	auto& c = tcp[h];

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_NETWORK);
		return;
	}

	sock_setIntOption(s, IPPROTO_TCP, TCP_NODELAY);
	sock_setNonBlocking(s);

	TcpState newState = TcpState::Closed;
	uint16_t localPort = 0;

	if (passive) {
		// Passive: bind to local port, then listen
		if (localPortReq == 0xFFFF) localPortReq = 0; // ephemeral, as in UDP_OPEN
		// Allow address reuse (common for servers)
		sock_setIntOption(s, SOL_SOCKET, SO_REUSEADDR);
		sockaddr_in addr = sock_makeIPv4(INADDR_ANY, localPortReq);
		if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
			sock_close(s);
			replyStatus(ERR_NO_NETWORK);
			return;
		}
		if (listen(s, 1) < 0) {
			sock_close(s);
			replyStatus(ERR_NO_NETWORK);
			return;
		}
		newState = TcpState::Listen;

		// Read back actual local port
		::socklen_t alen = sizeof(addr);
		if (getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &alen) == 0) {
			localPort = ntohs(addr.sin_port);
		} else {
			localPort = localPortReq;
		}
	} else {
		// Active connect. Remote IP/port are not semantically validated: a
		// connect to 0.0.0.0 or port 0 is handed to the host and fails there.
		sockaddr_in dest = sock_makeIPv4(ip, remotePort);
		int ret = connect(s, reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));
		if (ret == 0) {
			newState = TcpState::Established;
		} else {
#ifdef _WIN32
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
#else
			if (errno == EINPROGRESS) {
#endif
				newState = TcpState::SynSent;
			} else {
				sock_close(s);
				replyStatus(ERR_NO_NETWORK);
				return;
			}
		}
		// Read back the local port the OS assigned
		struct sockaddr_in local;
		::socklen_t len = sizeof(local);
		if (getsockname(s, reinterpret_cast<struct sockaddr*>(&local), &len) == 0) {
			localPort = ntohs(local.sin_port);
		}
	}

	{
		// Publish the connection in one go, with c.sock LAST: the socket thread
		// thread only looks at a connection once its socket is valid, so it
		// can never see a state without the matching address. Clearing the
		// buffers here is what destroys a reused slot's sticky tail.
		std::scoped_lock lock(c.mutex);
		c.closeReason = CloseReason::None;
		c.remoteIp    = ip;   // 0 = any; otherwise the socket thread filters on it
		c.remotePort  = remotePort;
		c.localPort   = localPort;
		c.resident    = resident;
		c.recvBuf.clear();
		c.sendBuf.clear();
		c.finSent = false;
		c.tcpState    = newState;
		c.sock        = s;
	}
	poke(); // start watching the new socket now

	setResult(OpenResult{.handle = static_cast<uint8_t>(h + 1)}); // wire handles are 1-based
}

// TCP_SEND (0x04)
// Params: handle[1] + len[2 LE] + data[len] - len states a fact about the
// payload and must be exact; beyond MAX_TRANSFER it is ERR_INV_PARAM
// Reply: {0}; ERR_BUFFER is all-or-nothing (retry the same command later)

void UnapiNet::cmdTcpSend()
{
	if (paramBuf.size() < sizeof(TcpSendParamHeader)) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	auto ph = fromBytes<TcpSendParamHeader>(paramBuf);
	size_t len = ph.len;
	if (paramBuf.size() != sizeof(TcpSendParamHeader) + len ||
	    len > MAX_TRANSFER) {
		replyStatus(ERR_INV_PARAM);
		return;
	}

	auto* cp = tcpForHandle(ph.handle);
	if (!cp) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	auto& c = *cp;

	// Check and act under the connection lock: without it the socket thread could
	// close the socket between the check and the send.
	const auto* data = paramBuf.data() + sizeof(TcpSendParamHeader);
	bool failed = false;
	bool queued = false; // bytes left in sendBuf for the socket thread
	uint8_t status = ERR_OK;
	{
		std::scoped_lock lock(c.mutex);
		SOCKET sd = c.sock;
		if (sd == OPENMSX_INVALID_SOCKET) {
			replyStatus(ERR_NO_CONN); // no connection at all
			return;
		}
		// FinWait1 means the MSX already closed its side: no more sending.
		if (c.tcpState != one_of(TcpState::Established, TcpState::CloseWait)) {
			replyStatus(ERR_CONN_STATE); // a connection, in a state that forbids it
			return;
		}

		if (!c.sendBuf.empty()) {
			// Data is already queued ahead of us: everything goes behind it,
			// all or nothing - a rejected command was not partially queued
			// and may be retried verbatim.
			if (c.sendBuf.size() + len > MAX_SEND_BUF) {
				status = ERR_BUFFER;
			} else {
				c.sendBuf.insert(c.sendBuf.end(), data, data + len);
				queued = true;
			}
		} else {
			// Nothing queued ahead of us: hand it straight to the kernel. The
			// socket is non-blocking, so this cannot stall the emulation
			// thread. Whatever the kernel would not take goes to the socket
			// thread, which drains it as the peer makes room - and it always
			// fits, because len <= MAX_TRANSFER <= MAX_SEND_BUF.
			auto r = netSend(sd, data, len);
			if (r || r.error() == IoError::WouldBlock) {
				size_t sent = r.value_or(0);
				c.sendBuf.insert(c.sendBuf.end(), data + sent, data + len);
				queued = sent < len;
			} else {
				failed = true;
			}
		}
	}
	if (failed) {
		// A socket write failure resets the connection; the reply reports
		// the state change, not a private send error code.
		requestClose(c, CloseReason::ConnectionReset);
		replyStatus(ERR_CONN_STATE);
		return;
	}
	if (queued) poke(); // arm this socket for write now, not at the
	                    // next select() timeout
	replyStatus(status);
}

// TCP_RECV (0x05)
// Params: handle[1] + maxlen[2 LE] - maxlen is a ceiling request: beyond
// MAX_TRANSFER it is clamped, not rejected; 0 reads nothing and consumes
// nothing
// Reply: {0, actual_len[2 LE], data[actual_len]}

void UnapiNet::cmdTcpRecv()
{
	auto p = exactParams<TcpRecvParams>();
	if (!p) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	auto* cp = tcpForHandle(p->handle);
	if (!cp) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	auto& c = *cp;
	// Answers for any in-range handle, open or closed: the undrained tail
	// of a dead connection stays readable until TCP_OPEN reuses the slot.

	size_t maxlen = std::min(static_cast<size_t>(p->maxlen), MAX_TRANSFER);

	// Build the whole result under the connection lock: the length in the
	// header and the bytes copied behind it must agree, and requestClose()
	// runs on another thread. setResult() does the resultPos bookkeeping;
	// appending the payload afterwards doesn't disturb it.
	// Nothing allocates while the lock is held: resultBuf was reserved in
	// the constructor and avail <= MAX_TRANSFER.
	std::scoped_lock lock(c.mutex);
	auto avail = static_cast<uint16_t>(std::min(maxlen, c.recvBuf.size()));
	setResult(TcpRecvResultHeader{.actualLen = avail});
	resultBuf.insert(resultBuf.end(), c.recvBuf.begin(),
	                 c.recvBuf.begin() + avail);
	c.recvBuf.erase(c.recvBuf.begin(), c.recvBuf.begin() + avail);
}

// TCP_CLOSE (0x06)
// Params: handle[1]; 0 = close all transient (succeeds even with none open)
// Reply: {0}

void UnapiNet::cmdTcpClose()
{
	if (paramBuf.size() != 1) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	int h = paramBuf[0];

	if (h == 0) {
		// Close all transient connections. Gracefully: data the MSX has
		// already handed us must not be thrown away.
		for (auto& c : tcp) {
			if (!c.resident && c.sock != OPENMSX_INVALID_SOCKET) {
				gracefulClose(c);
			}
		}
		replyStatus(ERR_OK);
		return;
	}

	auto* cp = tcpForHandle(h);
	if (!cp || cp->sock == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	gracefulClose(*cp);
	replyStatus(ERR_OK);
}

// TCP_STATE (0x07)
// Params: handle[1]
// Reply: {0, state[1], avail[2 LE], close_reason[1], remote_IP[4],
//         remote_port[2 LE], local_port[2 LE]} - answers for any in-range
// handle: a closed slot keeps its final state and close reason, a
// never-used one shows closeReason 1 (NeverUsed) and zeros

void UnapiNet::cmdTcpState()
{
	if (paramBuf.size() != 1) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	auto* cp = tcpForHandle(paramBuf[0]);
	if (!cp) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	auto& c = *cp;

	TcpStateResult r{};
	{
		// One coherent snapshot: the socket thread publishes the state
		// and the endpoint metadata together under this same lock.
		std::scoped_lock lock(c.mutex);
		r.state       = static_cast<uint8_t>(c.tcpState.load());
		r.avail       = static_cast<uint16_t>(
			std::min(c.recvBuf.size(), static_cast<size_t>(0xFFFF)));
		r.closeReason = static_cast<uint8_t>(c.closeReason);
		r.remoteIp    = c.remoteIp;
		r.remotePort  = c.remotePort;
		r.localPort   = c.localPort;
	}
	setResult(r);
}

// TCP_ABORT (0x08)
// Params: handle[1]; 0 = abort all transient (UNAPI's TCPIP_TCP_ABORT B=0)
// Reply: {0} - the aborted slot's tail stays readable (sticky-slot rule)

void UnapiNet::cmdTcpAbort()
{
	if (paramBuf.size() != 1) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	int h = paramBuf[0];

	if (h == 0) {
		// Abort every transient connection; succeeds even with none open.
		for (auto& c : tcp) {
			if (!c.resident && c.sock != OPENMSX_INVALID_SOCKET) {
				requestClose(c, CloseReason::Aborted);
			}
		}
		replyStatus(ERR_OK);
		return;
	}

	auto* cp = tcpForHandle(h);
	if (!cp || cp->sock == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	requestClose(*cp, CloseReason::Aborted);
	replyStatus(ERR_OK);
}

// GET_LOCALIP (0x0D)
// Params: none
// Reply: {0, ip4} - 0.0.0.0 when the host lookup fails, still a success

void UnapiNet::cmdGetLocalIP()
{
	if (!paramBuf.empty()) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	setResult(GetLocalIpResult{.ip = sock_localIPv4()});
}

// NET_STATE (0x0E)
// Params: none
// Reply: {0, 2} - always 'open': the host's network is ours

void UnapiNet::cmdNetState()
{
	if (!paramBuf.empty()) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	const std::array<uint8_t, 2> r{ERR_OK, 2};
	setResult(r);
}

// UDP: handle management

int UnapiNet::allocUdpHandle()
{
	for (int i = 0; i < MAX_UDP; i++) {
		if (udp[i].sock == OPENMSX_INVALID_SOCKET) {
			return i;
		}
	}
	return INVALID_HANDLE;
}

// Direct close. Only safe with the socket thread stopped (destructor).
void UnapiNet::closeUdp(UdpConnection& u)
{
	std::scoped_lock lock(u.mutex);
	if (SOCKET sd = u.sock; sd != OPENMSX_INVALID_SOCKET) {
		sock_close(sd);
		u.sock = OPENMSX_INVALID_SOCKET;
	}
	u.localPort = 0;
	u.resident = false;
	u.recvQueue.clear();
}

// UDP_OPEN (0x09)
// Params: local_port[2 LE] - 0xFFFF requests an ephemeral port; 0 reaches
// the host bind() where it also yields one, by a different route
// Reply: {0, handle}

void UnapiNet::cmdUdpOpen()
{
	auto p = exactParams<UdpOpenParams>();
	if (!p) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	uint16_t localPort = p->localPort;

	// Free-slot check before the host socket work, as in TCP_OPEN.
	int h = allocUdpHandle();
	if (h == INVALID_HANDLE) {
		replyStatus(ERR_NO_FREE_CONN);
		return;
	}
	auto& u = udp[h];

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_NETWORK);
		return;
	}

	sock_setNonBlocking(s);

	// Enable broadcast - required by UNAPI clients that do LAN service
	// discovery via sendto(255.255.255.255). Real-hardware UNAPI stacks
	// (GR8NET, Obsonet) allow it implicitly; the BSD socket layer needs
	// the explicit opt-in.
	sock_setIntOption(s, SOL_SOCKET, SO_BROADCAST);

	sockaddr_in addr = sock_makeIPv4(INADDR_ANY, localPort == 0xFFFF ? 0 : localPort);
	if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
		// The requested port may be taken by the HOST itself - Windows'
		// time service owns UDP 123, which is exactly what an SNTP client
		// asks for - a collision that cannot exist on the real-hardware
		// UNAPI stacks this bridge stands in for, which own their whole
		// IP. A client's local port carries no meaning for the peer, so
		// fall back to an ephemeral one rather than refuse a socket the
		// program needs.
		addr.sin_port = 0;
		if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
			sock_close(s);
			replyStatus(ERR_NO_NETWORK);
			return;
		}
	}

	// Read back the actual local port
	const auto boundPort = [&]() -> uint16_t {
		::socklen_t alen = sizeof(addr);
		return (getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &alen) == 0)
			? ntohs(addr.sin_port)
			: localPort;
	}();

	{
		// Publish with u.sock last (see the threading contract in the header).
		std::scoped_lock lock(u.mutex);
		u.localPort = boundPort;
		u.resident  = false;
		u.recvQueue.clear();
		u.sock      = s;
	}
	poke(); // start watching the new socket now

	setResult(OpenResult{.handle = static_cast<uint8_t>(h + 1)}); // wire handles are 1-based
}

// UDP_CLOSE (0x0A)
// Params: handle[1]; 0 = close every open UDP socket (all are transient)
// Reply: {0} - queued datagrams are discarded with the socket

void UnapiNet::cmdUdpClose()
{
	if (paramBuf.size() != 1) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	int h = paramBuf[0];

	if (h == 0) {
		for (auto& u : udp) {
			if (!u.resident && u.sock != OPENMSX_INVALID_SOCKET) {
				requestClose(u);
			}
		}
		replyStatus(ERR_OK);
		return;
	}

	auto* up = udpForHandle(h);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	requestClose(*up);
	replyStatus(ERR_OK);
}

// UDP_STATE (0x0B)
// Params: handle[1]
// Reply: {0, first_dgram_size[2 LE]} - 0 if the queue is empty, or if
// an empty datagram heads it; UDP_RECV until ERR_NO_DATA is the UNAPI
// idiom that tells the two apart (TCPIP_UDP_STATE's own advice)

void UnapiNet::cmdUdpState()
{
	if (paramBuf.size() != 1) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	auto* up = udpForHandle(paramBuf[0]);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	auto& u = *up;

	const auto size = [&]() -> uint16_t {
		std::scoped_lock lock(u.mutex);
		if (u.recvQueue.empty()) return 0;
		return static_cast<uint16_t>(
			std::min(u.recvQueue.front().data.size(),
			         static_cast<size_t>(0xFFFF)));
	}();
	setResult(UdpStateResult{.firstDgramSize = size});
}

// UDP_SEND (0x0C)
// Params: handle[1] + dest_IP[4] + dest_port[2 LE] + len[2 LE] + data[len]
// Reply: {0} - length 0 sends an empty datagram; an oversized one is not a
// form error: the host refuses it ({2}) and the socket stays usable

void UnapiNet::cmdUdpSend()
{
	if (paramBuf.size() < sizeof(UdpSendParamHeader)) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	auto ph = fromBytes<UdpSendParamHeader>(paramBuf);
	size_t len = ph.len;
	if (paramBuf.size() != sizeof(UdpSendParamHeader) + len) {
		replyStatus(ERR_INV_PARAM);
		return;
	}

	auto* up = udpForHandle(ph.handle);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	auto& u = *up;

	sockaddr_in dest = sock_makeIPv4(ph.destIp, ph.destPort);
	const char* data = reinterpret_cast<const char*>(
		paramBuf.data() + sizeof(UdpSendParamHeader));
	int n;
	{
		// Snapshot the socket under the lock: the socket thread may be closing it.
		std::scoped_lock lock(u.mutex);
		SOCKET sd = u.sock;
		if (sd == OPENMSX_INVALID_SOCKET) {
			replyStatus(ERR_NO_CONN);
			return;
		}
		n = sendto(sd, data, static_cast<int>(len), 0,
		           reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));
	}
	// A refused or short kernel write leaves the socket open and usable.
	replyStatus(n == static_cast<int>(len) ? ERR_OK : ERR_NO_NETWORK);
}

// UDP_RECV (0x0F)
// Params: handle[1] + maxlen[2 LE] - maxlen 0 is TCPIP_UDP_RCV's DE=0,
// a deliberate discard: consume the head, copy nothing
// Reply: {0, src_IP[4], src_port[2 LE], actual_len[2 LE],
//         data[min(actual_len, maxlen)]} - actual_len is the datagram's
// size as received (UNAPI's BC may exceed the bytes retrieved)

void UnapiNet::cmdUdpRecv()
{
	auto p = exactParams<UdpRecvParams>();
	if (!p) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	uint16_t maxlen = p->maxlen;
	auto* up = udpForHandle(p->handle);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		replyStatus(ERR_NO_CONN);
		return;
	}
	auto& u = *up;

	std::optional<UdpDatagram> dg;
	{
		std::scoped_lock lock(u.mutex);
		if (!u.recvQueue.empty()) {
			dg = std::move(u.recvQueue.front());
			u.recvQueue.pop_front();
		}
	}
	if (!dg) {
		replyStatus(ERR_NO_DATA);
		return;
	}

	// The head datagram is consumed whole; the reply reports its size as
	// received and carries the first min(size, maxlen) bytes - whatever
	// exceeds maxlen is discarded with it. This is what lets the driver
	// report TCPIP_UDP_RCV's BC ("size as it was received, which may be
	// larger than the number of bytes actually retrieved") without a
	// second command.
	auto copied = std::min(static_cast<size_t>(maxlen), dg->data.size());
	setResult(UdpRecvResultHeader{
			.srcIp     = dg->srcIp,
			.srcPort   = dg->srcPort,
			.actualLen = static_cast<uint16_t>(dg->data.size())},
		std::span<const uint8_t>(dg->data.data(), copied));
}

// ICMP Echo (ping)
//
// Uses the Windows IcmpSendEcho API (doesn't require admin). A worker
// thread handles the (blocking) call and pushes replies to a queue that
// the MSX polls via ICMP_RECV. The worker only runs when the capability
// latched at device start; elsewhere the ICMP commands answer ERR_NOT_IMP.

void UnapiNet::icmpWorkerLoop()
{
#ifdef _WIN32
	HANDLE hIcmp = icmpChannel; // opened (and the capability latched) at device start

	std::unique_lock lock(icmpMutex);
	while (true) {
		icmpCv.wait(lock, [&] { return !running || icmpPending; });
		if (!running) return;
		icmpPending = false;
		IcmpRequest req = icmpRequest; // copy out under the lock: a new
		                               // ICMP_SEND cannot tear it
		uint32_t gen = icmpGeneration;
		lock.unlock();

		std::vector<uint8_t> payload(req.dataLen);
		for (size_t i = 0; i < payload.size(); i++)
			payload[i] = static_cast<uint8_t>(i);

		unsigned long replySize = sizeof(ICMP_ECHO_REPLY) + req.dataLen + 8;
		std::vector<uint8_t> replyBuf(replySize);

		IP_OPTION_INFORMATION opt = {};
		opt.Ttl = req.ttl ? req.ttl : 255;

		unsigned long ret = IcmpSendEcho(hIcmp,
										 htonl(req.dstIp),
										 payload.empty() ? nullptr : payload.data(),
										 static_cast<unsigned short>(payload.size()),
										 &opt,
										 replyBuf.data(),
										 replySize,
										 2000);

		std::optional<IcmpReply> r;
		if (ret > 0) {
			auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf.data());
			if (reply->Status == IP_SUCCESS) {
				r = IcmpReply{
					.srcIp      = ntohl(reply->Address),
					.ttl        = reply->Options.Ttl,
					.identifier = req.identifier,
					.sequence   = req.sequence,
					.dataLen    = reply->DataSize};
			}
		}

		lock.lock(); // held again for the publish and the next wait
		if (r && icmpGeneration == gen) { // else: a reset intervened
			                              // while the echo was in flight
			if (icmpReplies.size() >= MAX_ICMP_QUEUE) {
				// Drop the OLDEST: a reply nobody polled for is worth
				// less than the one the current program is waiting for.
				icmpReplies.pop_front();
			}
			icmpReplies.push_back(*r);
		}
	}
#endif
}

// ICMP_SEND (0x11)
// Params: IP[4] + TTL[1] + ID[2 LE] + SEQ[2 LE] + len[2 LE]
// Reply: {0} - acknowledges queueing to the worker, not delivery: an
// unreachable destination simply never yields a reply

void UnapiNet::cmdIcmpSend()
{
	auto p = exactParams<IcmpSendParams>();
	if (!p) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	if (!icmpAvailable) {
		// The capability bit is enforced, not decorative.
		replyStatus(ERR_NOT_IMP);
		return;
	}
	{
		// Under the lock: the worker may be copying the previous request
		// out right now.
		std::scoped_lock lock(icmpMutex);
		icmpRequest.dstIp      = p->dstIp;
		icmpRequest.ttl        = p->ttl;
		icmpRequest.identifier = p->identifier;
		icmpRequest.sequence   = p->sequence;
		icmpRequest.dataLen    = std::min<uint16_t>(p->len, 512); // echo-size clamp
		icmpPending = true;
	}
	icmpCv.notify_one();
	replyStatus(ERR_OK);
}

// ICMP_RECV (0x12)
// Params: none
// Reply: {0, src_IP[4], TTL[1], ID[2 LE], SEQ[2 LE], len[2 LE]} - consumes
// exactly one queued reply; matching id/seq to requests is the driver's job

void UnapiNet::cmdIcmpRecv()
{
	if (!paramBuf.empty()) {
		replyStatus(ERR_INV_PARAM);
		return;
	}
	if (!icmpAvailable) {
		replyStatus(ERR_NOT_IMP);
		return;
	}

	std::optional<IcmpReply> r;
	{
		std::scoped_lock lock(icmpMutex);
		if (!icmpReplies.empty()) {
			r = icmpReplies.front();
			icmpReplies.pop_front();
		}
	}
	if (!r) {
		replyStatus(ERR_NO_DATA);
		return;
	}

	setResult(IcmpRecvResult{
		.srcIp      = r->srcIp,
		.ttl        = r->ttl,
		.identifier = r->identifier,
		.sequence   = r->sequence,
		.dataLen    = r->dataLen});
}

// Serialization (save state)
// We don't serialize sockets or network state.
// On restore, connections are lost.

template<typename Archive>
void UnapiNet::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// Don't serialize network state - connections are lost on save/load
}

INSTANTIATE_SERIALIZE_METHODS(UnapiNet);
REGISTER_MSXDEVICE(UnapiNet, "UnapiNet");

} // namespace openmsx

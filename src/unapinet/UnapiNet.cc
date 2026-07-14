#include "UnapiNet.hh"

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

// Socket handles are openMSX SOCKET values (see Socket.hh); no casts needed.

// UnapiNet - openMSX Extension (Phase 2)
//
// Bridge between the MSX I/O ports and BSD sockets on the host.
// Supports: async DNS, TCP (up to 4 simultaneous connections),
// circular receive buffer with a background thread.

namespace openmsx {

// --- Capabilities for QUERY_CAP (Phase 2) ---
// Byte 0: bit0=PING bit1=DNS_HOSTS(local) bit2=DNS bit3=TCP_ACTIVE... (see spec)
// Bits 0,2,3,10 = PING + DNS + TCP active + UDP
// Byte 1: bridge version
static constexpr uint8_t CAP_BYTE0 = 0x0F; // PING + DNS + TCP + UDP caps summary
static constexpr uint8_t CAP_BYTE1 = 0x04; // bridge version 4

// Maximum result payload per TCP_RECV command. TCP_SEND is not clamped to
// this; the driver keeps to the same bound by convention.
static constexpr size_t MAX_TRANSFER = 4096;

// Maximum receive buffer size per connection
// BBS ANSI screens can be 16-32KB, needs big buffer
static constexpr size_t MAX_RECV_BUF = 65536;

// Upper bound on accumulated command parameters. Large enough for any legal
// command (16-bit payload length + header); a runaway MSX program hammering
// the data port must not be able to exhaust host memory.
static constexpr size_t MAX_PARAM_BUF = 64 * 1024 + 16;

// Bound on data queued for sending but not yet accepted by the kernel.
// TCP_SEND reports 'buffer full' rather than blocking the emulation thread.
static constexpr size_t MAX_SEND_BUF = 64 * 1024;

// Bridge command opcodes (wire protocol, shared with the Z80 driver)
static constexpr uint8_t CMD_PING        = 0x00;
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
static constexpr uint8_t CMD_QUERY_CAP   = 0x10;
static constexpr uint8_t CMD_ICMP_SEND   = 0x11;
static constexpr uint8_t CMD_ICMP_RECV   = 0x12;

// PING reply magic
static constexpr uint8_t MAGIC = 0xAB;

// How long a half-closed connection may wait for the peer to close too,
// before we drop it and free the handle.
static constexpr auto CLOSE_TIMEOUT = std::chrono::seconds(30);

// openMSX's sock_recv()/sock_send() fold 'peer closed' and 'error' into -1,
// and decide would-block from SO_ERROR - which a synchronous WSAEWOULDBLOCK
// does not set on Windows. We need the three outcomes apart, so we call
// recv()/send() here and ask the platform ourselves.
enum class IoStatus { Ok, WouldBlock, Closed, Error };
struct IoResult {
	size_t bytes = 0;
	IoStatus status = IoStatus::Error;
};

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
	if (n > 0) return {static_cast<size_t>(n), IoStatus::Ok};
	if (n == 0) return {0, IoStatus::Closed}; // orderly shutdown by the peer
	return {0, ioWouldBlock() ? IoStatus::WouldBlock : IoStatus::Error};
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
	if (n >= 0) return {static_cast<size_t>(n), IoStatus::Ok};
	return {0, ioWouldBlock() ? IoStatus::WouldBlock : IoStatus::Error};
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

// UNAPI DNS error code reported on a failed lookup (host name does not exist)
static constexpr uint8_t DNS_ERR_NO_SUCH_HOST = 3;

// Status register values (wire protocol)
static constexpr uint8_t STATUS_OK    = 0x00;
static constexpr uint8_t STATUS_ERROR = 0x01;
static constexpr uint8_t STATUS_DATA  = 0x02;

// Constructor / Destructor

UnapiNet::UnapiNet(const DeviceConfig& config)
	: MSXDevice(config)
{
	paramBuf.reserve(MAX_TRANSFER + 16);
	resultBuf.reserve(MAX_TRANSFER + 16);

	reset(EmuTime::dummy()); // keep constructor and reset() in sync

	// Start background threads
	running = true;
	recvThread = std::thread([this]() { receiverLoop(); });
	icmpWorker = std::thread([this]() { icmpWorkerLoop(); });
}

UnapiNet::~UnapiNet()
{
	running = false;
	if (recvThread.joinable()) recvThread.join();
	if (icmpWorker.joinable()) icmpWorker.join();
	// The DNS lookup is blocking: joining it is deliberate, so getaddrinfo()
	// cannot write into a destroyed object.
	if (dnsThread.joinable())  dnsThread.join();
	closeAllConnections();
	// The receiver stopped without draining its queue: close what is left,
	// or we leak every fd handed over in the last select cycle.
	{
		std::scoped_lock lock(closeMutex);
		for (SOCKET sd : socksToClose) sock_close(sd);
		socksToClose.clear();
	}
}

// Reset

void UnapiNet::reset(EmuTime /*time*/)
{
	statusReg = STATUS_OK;
	paramBuf.clear();
	resultBuf.clear();
	resultPos = 0;

	// Ask the receiver thread to drop everything (it owns the sock_close()).
	// During construction the thread doesn't exist yet - but neither do any
	// sockets, so this is a no-op then.
	for (auto& c : tcp) requestClose(c, CloseReason::NeverUsed, true);
	for (auto& u : udp) requestClose(u);

	dns.status = DnsStatus::Idle;
	dns.resolvedIp = 0;
}

// Port reads

byte UnapiNet::peekIO(uint16_t port, EmuTime /*time*/) const
{
	if (port & 1) {
		// data register (typically 0x29)
		if (statusReg == STATUS_DATA && resultPos < resultBuf.size()) {
			return resultBuf[resultPos];
		}
		return 0x00;
	} else {
		// status register (typically 0x28)
		return statusReg;
	}
}

byte UnapiNet::readIO(uint16_t port, EmuTime time)
{
	byte b = peekIO(port, time);
	if (port & 1) {
		// reading the data register consumes one result byte
		if (statusReg == STATUS_DATA && resultPos < resultBuf.size()) {
			if (++resultPos >= resultBuf.size()) {
				statusReg = STATUS_OK; // result fully consumed
			}
		}
	}
	return b;
}

// Port writes

void UnapiNet::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	if (port & 1) {
		// parameter (accumulate), typically 0x29
		// If there is a pending unread result, discard it
		// so that the new parameters are accepted
		if (statusReg == STATUS_DATA) {
			statusReg = STATUS_OK;
			resultBuf.clear();
			resultPos = 0;
		}
		if (paramBuf.size() < MAX_PARAM_BUF) {
			paramBuf.push_back(value);
		}
		// else: drop the byte; the command's size checks will report an
		// error to the MSX, and host memory stays bounded
	} else {
		// command (typically 0x28)
		processCmd(value);
	}
}

// Result helpers

void UnapiNet::setResult(std::span<const uint8_t> data)
{
	resultBuf.assign(data.begin(), data.end());
	resultPos = 0;
	statusReg = STATUS_DATA;
}

void UnapiNet::setResultByte(uint8_t b)
{
	setResult(std::span<const uint8_t>(&b, 1));
}

void UnapiNet::setError()
{
	resultBuf.clear();
	resultPos = 0;
	statusReg = STATUS_ERROR;
}

// TCP handle management

int UnapiNet::allocTcpHandle()
{
	for (int i = 0; i < MAX_TCP; i++) {
		if (tcp[i].sock == OPENMSX_INVALID_SOCKET &&
			tcp[i].tcpState == TcpState::Closed) {
			return i;
		}
	}
	return INVALID_HANDLE;
}

// The wire handle is a byte from the MSX: 1-based, 0 = error. These two are
// the only places that map wire handles to the 0-based internal arrays.
UnapiNet::TcpConnection* UnapiNet::tcpForHandle(int wireHandle)
{
	return (wireHandle >= 1 && wireHandle <= MAX_TCP) ? &tcp[wireHandle - 1] : nullptr;
}

UnapiNet::UdpConnection* UnapiNet::udpForHandle(int wireHandle)
{
	return (wireHandle >= 1 && wireHandle <= MAX_UDP) ? &udp[wireHandle - 1] : nullptr;
}

// Direct close. Only safe with the receiver thread stopped (destructor).
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

// Network receiver thread (background)
//
// Polls all active TCP sockets and moves incoming data into each
// connection's recvBuf. Also detects completion of a non-blocking
// connect() and state transitions (remote close, etc.).

void UnapiNet::deferSockClose(SOCKET sd)
{
	std::scoped_lock lock(closeMutex);
	socksToClose.push_back(sd);
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
		if (clearMetadata) {
			c.remoteIp   = 0;
			c.remotePort = 0;
			c.localPort  = 0;
			c.resident   = false;
			c.recvBuf.clear();
		}
	}
	if (sd != OPENMSX_INVALID_SOCKET) deferSockClose(sd);
}

void UnapiNet::requestClose(UdpConnection& u)
{
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
// CLOSE_TIMEOUT passes and it never does).
void UnapiNet::gracefulClose(TcpConnection& c)
{
	{
		// Test and act under one lock: the receiver may have dropped this
		// connection (peer reset) between the two, which would leave FinWait1
		// stamped on a slot that has no socket - unusable and unrecoverable.
		std::scoped_lock lock(c.mutex);
		SOCKET sd = c.sock;
		if (sd != OPENMSX_INVALID_SOCKET &&
		    c.tcpState == one_of(TcpState::Established, TcpState::CloseWait)) {
			c.closeReason = CloseReason::ClosedByUser;
			c.tcpState = TcpState::FinWait1;
			c.closeDeadline = std::chrono::steady_clock::now() + CLOSE_TIMEOUT;
			c.finSent = false;
			if (c.sendBuf.empty()) {
				shutdownSend(sd);
				c.finSent = true;
			}
			return;
		}
	} // drop the lock: requestClose() takes it
	// Nothing to half-close (listening, still connecting, or already gone).
	requestClose(c, CloseReason::ClosedByUser, true);
}

void UnapiNet::receiverLoop()
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

		// Wait on all active sockets at once with a single select() (short
		// timeout so we periodically re-check 'running' and pick up newly
		// opened sockets), instead of busy-polling each socket in turn.
		fd_set rfds;
		fd_set wfds;
		fd_set efds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);
		// Empty optional = no socket open. An empty optional compares less
		// than any engaged one, so std::max() works unchanged - and unlike a
		// sentinel it cannot collide with a real descriptor (fd 0 is valid on
		// POSIX, and on Windows INVALID_SOCKET is the LARGEST value).
		std::optional<SOCKET> maxSock;
		bool armed = false; // at least one fd went into one of the sets
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
					requestClose(c, CloseReason::ClosedByUser, true);
					continue;
				}
			}

			watchedTcp[i] = sd;
			if (c.tcpState == TcpState::SynSent) {
				// connect() completion shows up as writable (POSIX) or in
				// the exception set (Windows).
				FD_SET(sd, &wfds);
				FD_SET(sd, &efds);
				armed = true;
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
				if (hasRoom)     { FD_SET(sd, &rfds); armed = true; }
				if (pendingSend) { FD_SET(sd, &wfds); armed = true; }
			}
			maxSock = std::max(maxSock, std::optional(sd));
		}
		for (int i = 0; i < MAX_UDP; ++i) {
			auto& u = udp[i];
			SOCKET sd = u.sock;
			if (sd == OPENMSX_INVALID_SOCKET) continue;
			watchedUdp[i] = sd;
			FD_SET(sd, &rfds);
			maxSock = std::max(maxSock, std::optional(sd));
			armed = true;
		}
		if (!maxSock) {
			// Nothing open: avoid a tight loop (select() needs at least one fd).
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}
		if (!armed) {
			// Sockets are open, but every one of them is waiting for the MSX to
			// drain its recvBuf. Calling select() with three empty sets is an
			// error on Windows (WSAEINVAL), which would spin this loop, so wait
			// a moment instead. This costs no throughput: the MSX still has a
			// full buffer to work through before it needs more data.
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}
		struct timeval tv = {0, 100000}; // 100 ms
		if (select(static_cast<int>(*maxSock) + 1, &rfds, &wfds, &efds, &tv) <= 0) {
			continue; // timeout or error: re-check running and rebuild the set
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
					// and the remote-IP filter has to be read coherently.
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
						if (r.status == IoStatus::Error) { sendFailed = true; break; }
						if (r.bytes == 0) break; // would block
						c.sendBuf.erase(c.sendBuf.begin(),
						                c.sendBuf.begin() + r.bytes);
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
			switch (r.status) {
			case IoStatus::Ok: {
				std::scoped_lock lock(c.mutex);
				if (c.sock != sd) break;
				const auto* d = reinterpret_cast<const uint8_t*>(buf.data());
				c.recvBuf.insert(c.recvBuf.end(), d, d + r.bytes);
				break;
			}
			case IoStatus::Closed:
				// The peer closed its side. If we had closed ours too, the
				// connection is finished; otherwise the MSX may still send.
				if (c.tcpState == TcpState::FinWait1) {
					requestClose(c, CloseReason::ClosedByUser, true);
				} else {
					std::scoped_lock lock(c.mutex);
					if (c.sock == sd) c.tcpState = TcpState::CloseWait;
				}
				break;
			case IoStatus::WouldBlock:
				break; // spurious readable: nothing to do
			case IoStatus::Error:
				requestClose(c, CloseReason::ConnectionReset);
				break;
			}
		}

		for (int i = 0; i < MAX_UDP; ++i) {
			auto& u = udp[i];
			SOCKET sd = watchedUdp[i];
			if (sd == OPENMSX_INVALID_SOCKET || u.sock != sd) continue;
			if (!FD_ISSET(sd, &rfds)) continue;
			std::array<char, 2048> buf;
			struct sockaddr_in src;
			::socklen_t slen = sizeof(src);
			int n = recvfrom(sd, buf.data(), buf.size(), 0,
			                 reinterpret_cast<struct sockaddr*>(&src), &slen);
			if (n <= 0) continue;
			UdpDatagram dg;
			dg.srcIp = ntohl(src.sin_addr.s_addr);
			dg.srcPort = ntohs(src.sin_port);
			dg.data.assign(buf.data(), buf.data() + n);
			std::scoped_lock lock(u.mutex);
			if (u.sock != sd) continue;
			if (u.recvQueue.size() < 16) { // cap pending datagrams
				u.recvQueue.push_back(std::move(dg));
			}
		}
	}
}

// Command processing

void UnapiNet::processCmd(uint8_t cmd)
{
	switch (cmd) {
	case CMD_PING:       cmdPing();      break;
	case CMD_QUERY_CAP:  cmdQueryCap();  break;
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
		setError();
		break;
	}
	paramBuf.clear(); // always clear params after a command
}

// PING (0x00) - Phase 1

void UnapiNet::cmdPing()
{
	setResultByte(MAGIC);
}

// QUERY_CAP (0x10) - Phase 1+2

void UnapiNet::cmdQueryCap()
{
	setResult(QueryCapResult{.cap0 = CAP_BYTE0, .cap1 = CAP_BYTE1});
}

// DNS_QUERY (0x01)
// Params: hostname terminated with \0
// Result: 1 byte status
//   0 = resolution in progress (async)
//   1 = resolved immediately (IP in bytes 1-4)
//   2 = resolved locally
// Error if the hostname is empty or a query is already in progress

void UnapiNet::cmdDnsQuery()
{
	if (dns.status == DnsStatus::InProgress) {
		// A lookup is already running and owns dns.status/dns.resolvedIp.
		setError();
		return;
	}

	// The driver sends the hostname 0-terminated; the terminator and anything
	// after it are ignored. An empty parameter block yields an empty hostname,
	// which the check below rejects.
	std::string hostname(paramBuf.begin(), paramBuf.end());
	if (auto pos = hostname.find('\0'); pos != std::string::npos) {
		hostname.resize(pos);
	}

	if (hostname.empty()) {
		setError();
		return;
	}

	// Is it already an IP address? Try to parse it
	struct in_addr addr;
	if (inet_pton(AF_INET, hostname.c_str(), &addr) == 1) {
		// It's a direct IP
		uint32_t ip = ntohl(addr.s_addr);
		dns.resolvedIp = ip;
		dns.status = DnsStatus::Complete;

		setResult(DnsQueryResult{
			.status = 1, // resolved immediately
			.ip     = ip});
		return;
	}

	// Async resolution (the 'already busy' case returned at the top)
	dns.status = DnsStatus::InProgress;
	dns.resolvedIp = 0;

	// Wait for the previous DNS thread if it is still around
	if (dnsThread.joinable()) {
		dnsThread.join();
	}

	dnsThread = std::thread([this, hostname]() {
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		struct addrinfo* res = nullptr;
		int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
		if (err == 0 && res != nullptr) {
			auto* addr4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
			// Store IP in big-endian (network order) as-is
			// so bytes extract as octets: (ip>>24)=first, (ip>>0)=last
			dns.resolvedIp = ntohl(addr4->sin_addr.s_addr);
			dns.status = DnsStatus::Complete;
			freeaddrinfo(res);
		} else {
			if (res) freeaddrinfo(res);
			dns.status = DnsStatus::Error;
		}
	});

	setResultByte(0); // query in progress
}

// DNS_STATUS (0x02)
// Params: -
// Result: 1 byte status + if complete: 4 bytes IP
//   0 = idle (no query)
//   1 = in_progress
//   2 = complete -> +4 bytes IP
//   0xFF = error

void UnapiNet::cmdDnsStatus()
{
	DnsStatus s = dns.status; // implicit atomic load ('auto' would try to copy it)

	if (s == DnsStatus::Complete) {
		// Complete
		setResult(DnsStatusResult{
			.status = 2,
			.ip     = uint32_t(dns.resolvedIp)});
	} else if (s == DnsStatus::Error) {
		// Error
		// The only failure the resolver reports is 'host name does not exist'.
		setResult(DnsStatusError{.status = 0xFF, .errorCode = DNS_ERR_NO_SUCH_HOST});
	} else {
		// idle (0) or in_progress (1)
		setResultByte(static_cast<uint8_t>(s));
	}
}

// TCP_OPEN (0x03)
// Params: IP[4] + remote_port[2 LE] + local_port[2 LE] + timeout[2] + flags[1]
// Flags bit 0: passive mode (listen). bit 1: resident.
// Result: 1 byte handle (1-4, or 0 on error)

void UnapiNet::cmdTcpOpen()
{
	if (paramBuf.size() < sizeof(TcpOpenParams)) {
		setResultByte(0);
		return;
	}

	auto p = fromBytes<TcpOpenParams>(paramBuf);
	uint32_t ip           = p.remoteIp;
	uint16_t remotePort   = p.remotePort;
	uint16_t localPortReq = p.localPort;
	uint8_t  flags        = p.flags;
	bool passive  = (flags & 0x01) != 0;
	bool resident = (flags & 0x02) != 0;

	int h = allocTcpHandle();
	if (h == INVALID_HANDLE) {
		setResultByte(0);
		return;
	}

	auto& c = tcp[h];

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == OPENMSX_INVALID_SOCKET) {
		setResultByte(0);
		return;
	}

	sock_setIntOption(s, IPPROTO_TCP, TCP_NODELAY);
	sock_setNonBlocking(s);

	TcpState newState = TcpState::Closed;
	uint16_t localPort = 0;

	if (passive) {
		// Passive: bind to local port, then listen
		if (localPortReq == 0xFFFF) {
			// Shouldn't normally happen in passive mode, but handle it
			localPortReq = 0;
		}
		// Allow address reuse (common for servers)
		sock_setIntOption(s, SOL_SOCKET, SO_REUSEADDR);
		sockaddr_in addr = sock_makeIPv4(INADDR_ANY, localPortReq);
		if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
			sock_close(s);
			setResultByte(0);
			return;
		}
		if (listen(s, 1) < 0) {
			sock_close(s);
			setResultByte(0);
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
		// Active connect
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
				setResultByte(0);
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
		// Publish the connection in one go, with c.sock LAST: the receiver
		// thread only looks at a connection once its socket is valid, so it
		// can never see a state without the matching address.
		std::scoped_lock lock(c.mutex);
		c.closeReason = CloseReason::None;
		c.remoteIp    = ip;   // 0 = any; otherwise the receiver filters on it
		c.remotePort  = remotePort;
		c.localPort   = localPort;
		c.resident    = resident;
		c.recvBuf.clear();
		c.sendBuf.clear();
		c.finSent = false;
		c.tcpState    = newState;
		c.sock        = s;
	}

	setResultByte(static_cast<uint8_t>(h + 1)); // wire handles are 1-based
}

// TCP_SEND (0x04)
// Params: handle[1] + len[2 LE] + data[len]
// Result: 1 byte status (0=OK, 1=error)

void UnapiNet::cmdTcpSend()
{
	if (paramBuf.size() < sizeof(TcpSendParamHeader)) {
		setResultByte(1);
		return;
	}

	auto ph = fromBytes<TcpSendParamHeader>(paramBuf);
	int h = ph.handle;
	uint16_t len = ph.len;

	auto* cp = tcpForHandle(h);
	if (!cp) {
		setResultByte(1);
		return;
	}
	auto& c = *cp;

	if (paramBuf.size() < sizeof(TcpSendParamHeader) + len) {
		setResultByte(1);
		return;
	}

	// Check and act under the connection lock: without it the receiver could
	// close the socket between the check and the send.
	const auto* data = paramBuf.data() + sizeof(TcpSendParamHeader);
	bool failed = false;
	uint8_t result = 0;
	{
		std::scoped_lock lock(c.mutex);
		SOCKET sd = c.sock;
		// FinWait1 means the MSX already closed its side: no more sending.
		if (sd == OPENMSX_INVALID_SOCKET ||
		    c.tcpState != one_of(TcpState::Established, TcpState::CloseWait)) {
			setResultByte(1);
			return;
		}

		size_t sent = 0;
		if (c.sendBuf.empty()) {
			// Nothing queued ahead of us: hand it straight to the kernel. The
			// socket is non-blocking, so this cannot stall the emulation thread
			// and the data does not have to wait for the receiver's next pass.
			auto r = netSend(sd, data, len);
			if (r.status == IoStatus::Error) {
				failed = true;
			} else {
				sent = r.bytes;
			}
		}
		if (!failed && sent < len) {
			// Whatever the kernel would not take goes to the receiver thread,
			// which drains it as the peer makes room.
			size_t rest = size_t(len) - sent;
			if (c.sendBuf.size() + rest > MAX_SEND_BUF) {
				result = 2; // buffer full; the MSX should retry later
			} else {
				c.sendBuf.insert(c.sendBuf.end(), data + sent, data + len);
			}
		}
	}
	if (failed) {
		requestClose(c, CloseReason::ConnectionReset);
		setResultByte(1);
		return;
	}
	setResultByte(result); // 0 = accepted, 2 = buffer full
}

// TCP_RECV (0x05)
// Params: handle[1] + maxlen[2 LE]
// Result: actual_len[2 LE] + data[actual_len]

void UnapiNet::cmdTcpRecv()
{
	if (paramBuf.size() < sizeof(TcpRecvParams)) {
		setResult(TcpRecvResultHeader{}); // no data
		return;
	}

	auto p = fromBytes<TcpRecvParams>(paramBuf);
	int h = p.handle;
	uint16_t maxlen = p.maxlen;

	auto* cp = tcpForHandle(h);
	if (!cp) {
		setResult(TcpRecvResultHeader{});
		return;
	}
	auto& c = *cp;

	// Clamp to the maximum transfer size
	if (maxlen > MAX_TRANSFER) maxlen = static_cast<uint16_t>(MAX_TRANSFER);

	// Build the whole result under the connection lock: the length in the
	// header and the bytes copied behind it must agree, and requestClose()
	// clears recvBuf from another thread. setResult() does the resultPos /
	// statusReg bookkeeping; appending the payload afterwards doesn't
	// disturb it. Nothing allocates while the lock is held: resultBuf was
	// reserved in the constructor and avail <= MAX_TRANSFER.
	std::scoped_lock lock(c.mutex);
	auto avail = static_cast<uint16_t>(
		std::min(static_cast<size_t>(maxlen), c.recvBuf.size()));
	setResult(TcpRecvResultHeader{.actualLen = avail});
	resultBuf.insert(resultBuf.end(), c.recvBuf.begin(),
	                 c.recvBuf.begin() + avail);
	c.recvBuf.erase(c.recvBuf.begin(), c.recvBuf.begin() + avail);
}

// TCP_CLOSE (0x06)
// Params: handle[1]
// Result: 1 byte status (0=OK)

void UnapiNet::cmdTcpClose()
{
	if (paramBuf.empty()) {
		setResultByte(1);
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
		setResultByte(0);
		return;
	}

	auto* cp = tcpForHandle(h);
	if (!cp || cp->sock == OPENMSX_INVALID_SOCKET) {
		setResultByte(1);
		return;
	}
	gracefulClose(*cp);
	setResultByte(0);
}

// TCP_STATE (0x07)
// Params: handle[1]
// Result: state[1] + avail[2 LE] + close_reason[1] +
//         remote_IP[4] + remote_port[2 LE] + local_port[2 LE]
// (12 bytes total - the TSR only reads the first 4 unless HL != 0)

void UnapiNet::cmdTcpState()
{
	TcpStateResult r{};
	r.closeReason = static_cast<uint8_t>(CloseReason::NeverUsed); // default: no/invalid handle

	if (!paramBuf.empty()) {
		int h = paramBuf[0];
		if (auto* cp = tcpForHandle(h)) {
			auto& c = *cp;
			// One coherent snapshot: the receiver thread publishes the state
			// and the endpoint metadata together under this same lock.
			std::scoped_lock lock(c.mutex);
			TcpState state = c.tcpState;
			r.state       = static_cast<uint8_t>(state);
			r.avail       = static_cast<uint16_t>(
				std::min(c.recvBuf.size(), static_cast<size_t>(0xFFFF)));
			r.closeReason = static_cast<uint8_t>(c.closeReason);
			r.remoteIp    = c.remoteIp;
			r.remotePort  = c.remotePort;
			r.localPort   = c.localPort;
		}
	}
	setResult(r);
}

// TCP_ABORT (0x08)
// Params: handle[1]
// Result: 1 byte status (0=OK)

void UnapiNet::cmdTcpAbort()
{
	if (paramBuf.empty()) {
		setResultByte(1);
		return;
	}

	int h = paramBuf[0];
	auto* cp = tcpForHandle(h);
	if (!cp || cp->sock == OPENMSX_INVALID_SOCKET) {
		setResultByte(1);
		return;
	}

	requestClose(*cp, CloseReason::Aborted, true);
	setResultByte(0);
}

// GET_LOCALIP (0x0D)
// Params: -
// Result: 4 bytes IP (big-endian: a.b.c.d)

void UnapiNet::cmdGetLocalIP()
{
	setResult(GetLocalIpResult{.ip = sock_localIPv4()});
}

// NET_STATE (0x0E)
// Params: -
// Result: 1 byte (0=closed, 1=opening, 2=open, 3=closing)
// Always "open" because we use the host's network directly.

void UnapiNet::cmdNetState()
{
	setResultByte(2); // open
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

// Direct close. Only safe with the receiver thread stopped (destructor).
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
// Params: local_port[2 LE] (0xFFFF = random)
// Result: 1 byte handle (0 = error)

void UnapiNet::cmdUdpOpen()
{
	if (paramBuf.size() < sizeof(UdpOpenParams)) {
		setResultByte(0);
		return;
	}
	uint16_t localPort = fromBytes<UdpOpenParams>(paramBuf).localPort;

	int h = allocUdpHandle();
	if (h == INVALID_HANDLE) {
		setResultByte(0);
		return;
	}
	auto& u = udp[h];

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == OPENMSX_INVALID_SOCKET) {
		setResultByte(0);
		return;
	}

	sock_setNonBlocking(s);

	// Enable broadcast — required by UNAPI clients that do LAN service
	// discovery via sendto(255.255.255.255). Real-hardware UNAPI stacks
	// (GR8NET, Obsonet) allow it implicitly; the BSD socket layer needs
	// the explicit opt-in.
	sock_setIntOption(s, SOL_SOCKET, SO_BROADCAST);

	sockaddr_in addr = sock_makeIPv4(INADDR_ANY, localPort == 0xFFFF ? 0 : localPort);

	if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
		// Fallback: if bind to a privileged port (<1024) fails on Windows
		// without admin, fall back to a random port. MSX UDP clients
		// typically don't depend on a specific local port.
		addr.sin_port = 0;
		if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
			sock_close(s);
			setResultByte(0);
			return;
		}
	}

	// Read back the actual local port
	::socklen_t alen = sizeof(addr);
	uint16_t boundPort =
		(getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &alen) == 0)
			? ntohs(addr.sin_port)
			: localPort;

	{
		// Publish with u.sock last (see the threading contract in the header).
		std::scoped_lock lock(u.mutex);
		u.localPort = boundPort;
		u.resident  = false;
		u.recvQueue.clear();
		u.sock      = s;
	}

	setResultByte(static_cast<uint8_t>(h + 1)); // wire handles are 1-based
}

// UDP_CLOSE (0x0A)
// Params: handle[1] (0 = close all transient)
// Result: 1 byte status

void UnapiNet::cmdUdpClose()
{
	if (paramBuf.empty()) {
		setResultByte(1);
		return;
	}
	int h = paramBuf[0];

	if (h == 0) {
		for (auto& u : udp) {
			if (!u.resident && u.sock != OPENMSX_INVALID_SOCKET) {
				requestClose(u);
			}
		}
		setResultByte(0);
		return;
	}

	auto* up = udpForHandle(h);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		setResultByte(1);
		return;
	}
	requestClose(*up);
	setResultByte(0);
}

// UDP_STATE (0x0B)
// Params: handle[1]
// Result: 2 bytes: first datagram size (LE), 0 if none

void UnapiNet::cmdUdpState()
{
	uint16_t size = 0;
	if (!paramBuf.empty()) {
		int h = paramBuf[0];
		if (auto* up = udpForHandle(h); up && up->sock != OPENMSX_INVALID_SOCKET) {
			auto& u = *up;
			std::scoped_lock lock(u.mutex);
			if (!u.recvQueue.empty()) {
				size = static_cast<uint16_t>(
					std::min(u.recvQueue.front().data.size(),
							 static_cast<size_t>(0xFFFF)));
			}
		}
	}
	setResult(UdpStateResult{.firstDgramSize = size});
}

// UDP_SEND (0x0C)
// Params: handle[1] + dest_IP[4] + dest_port[2 LE] + len[2 LE] + data
// Result: 1 byte status

void UnapiNet::cmdUdpSend()
{
	if (paramBuf.size() < sizeof(UdpSendParamHeader)) {
		setResultByte(1);
		return;
	}
	auto ph = fromBytes<UdpSendParamHeader>(paramBuf);
	int h = ph.handle;
	auto* up = udpForHandle(h);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		setResultByte(1);
		return;
	}
	auto& u = *up;

	uint32_t ip   = ph.destIp;
	uint16_t port = ph.destPort;
	uint16_t len  = ph.len;

	if (paramBuf.size() < sizeof(UdpSendParamHeader) + len) {
		setResultByte(1);
		return;
	}

	sockaddr_in dest = sock_makeIPv4(ip, port);

	const char* data = reinterpret_cast<const char*>(paramBuf.data() + sizeof(UdpSendParamHeader));
	int n;
	{
		// Snapshot the socket under the lock: the receiver may be closing it.
		std::scoped_lock lock(u.mutex);
		SOCKET sd = u.sock;
		if (sd == OPENMSX_INVALID_SOCKET) {
			setResultByte(1);
			return;
		}
		n = sendto(sd, data, len, 0,
		           reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));
	}
	setResultByte(n == len ? 0 : 1);
}

// UDP_RECV (0x0F)
// Params: handle[1] + maxlen[2 LE]
// Result: src_IP[4] + src_port[2 LE] + actual_len[2 LE] + data

void UnapiNet::cmdUdpRecv()
{
	if (paramBuf.size() < sizeof(UdpRecvParams)) {
		setResult(UdpRecvResultHeader{});
		return;
	}
	auto p = fromBytes<UdpRecvParams>(paramBuf);
	int h = p.handle;
	uint16_t maxlen = p.maxlen;

	auto* up = udpForHandle(h);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		setResult(UdpRecvResultHeader{});
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
		setResult(UdpRecvResultHeader{});
		return;
	}

	uint16_t actual = static_cast<uint16_t>(
		std::min(static_cast<size_t>(maxlen), dg->data.size()));

	UdpRecvResultHeader hdr{
		.srcIp     = dg->srcIp,
		.srcPort   = dg->srcPort,
		.actualLen = actual};
	setResult(hdr, std::span<const uint8_t>(dg->data.data(), actual));
}

// ICMP Echo (ping)
//
// Uses the Windows IcmpSendEcho API (doesn't require admin).
// A worker thread handles the (blocking) call and pushes replies
// to a queue that the MSX polls via RCV_ECHO.

void UnapiNet::icmpWorkerLoop()
{
#ifdef _WIN32
	HANDLE hIcmp = IcmpCreateFile();
	if (hIcmp == INVALID_HANDLE_VALUE) return;

	while (running) {
		if (!icmpPending.exchange(false)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}

		IcmpRequest req = icmpRequest;

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

		if (ret > 0) {
			auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf.data());
			if (reply->Status == IP_SUCCESS) {
				IcmpReply r;
				r.srcIp = ntohl(reply->Address);
				r.ttl = reply->Options.Ttl;
				r.identifier = req.identifier;
				r.sequence = req.sequence;
				r.dataLen = reply->DataSize;
				std::scoped_lock lock(icmpMutex);
				if (icmpReplies.size() < 16) icmpReplies.push_back(r);
			}
		}
	}

	IcmpCloseHandle(hIcmp);
#endif
}

// SEND_ECHO (0x11)
// Params: IP[4] + TTL[1] + ID[2] + SEQ[2] + len[2]
// Result: 1 byte status
void UnapiNet::cmdIcmpSend()
{
	if (paramBuf.size() < sizeof(IcmpSendParams)) {
		setResultByte(1);
		return;
	}
	auto p = fromBytes<IcmpSendParams>(paramBuf);
	icmpRequest.dstIp      = p.dstIp;
	icmpRequest.ttl        = p.ttl;
	icmpRequest.identifier = p.identifier;
	icmpRequest.sequence   = p.sequence;
	icmpRequest.dataLen    = p.len;
	if (icmpRequest.dataLen > 512) icmpRequest.dataLen = 512;

	icmpPending = true;
	setResultByte(0); // OK: request queued
}

// RCV_ECHO (0x12)
// Params: -
// Result: 1 byte has_data + [if 1: IP(4)+TTL(1)+ID(2)+SEQ(2)+len(2) = 11 bytes]
void UnapiNet::cmdIcmpRecv()
{
	std::optional<IcmpReply> r;
	{
		std::scoped_lock lock(icmpMutex);
		if (!icmpReplies.empty()) {
			r = icmpReplies.front();
			icmpReplies.pop_front();
		}
	}

	if (!r) {
		setResultByte(0); // status 0 = no data
		return;
	}

	setResult(IcmpRecvResult{
		.hasData    = 1,
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

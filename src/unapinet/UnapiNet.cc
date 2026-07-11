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

// Maximum transfer size per TCP_SEND/TCP_RECV command
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
	if (dnsThread.joinable())  dnsThread.join();
	closeAllConnections();
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
	c.pendingClose = false;
	c.pendingShutdown = false;
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

void UnapiNet::requestClose(TcpConnection& c, CloseReason reason,
                            bool clearMetadata)
{
	std::scoped_lock lock(c.mutex);
	c.closeReason = reason;
	if (clearMetadata) {
		c.remoteIp   = 0;
		c.remotePort = 0;
		c.localPort  = 0;
		c.resident   = false;
		c.recvBuf.clear();
	}
	c.sendBuf.clear();
	c.pendingShutdown = false;
	c.tcpState = TcpState::Closed;
	if (c.sock != OPENMSX_INVALID_SOCKET) {
		c.pendingClose = true; // the receiver thread owns the sock_close()
	}
}

void UnapiNet::requestClose(UdpConnection& u)
{
	std::scoped_lock lock(u.mutex);
	u.localPort = 0;
	u.resident  = false;
	u.recvQueue.clear();
	if (u.sock != OPENMSX_INVALID_SOCKET) {
		u.pendingClose = true;
	}
}

void UnapiNet::receiverLoop()
{
	while (running) {
		// Wait on all active sockets at once with a single select() (short
		// timeout so we periodically re-check 'running' and pick up newly
		// opened sockets), instead of busy-polling each socket in turn.
		fd_set rfds;
		fd_set wfds;
		fd_set efds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);
		SOCKET maxSock = 0;
		for (auto& c : tcp) {
			// This thread is the only one that closes a live socket; the others
			// just ask via pendingClose.
			if (c.pendingClose) {
				std::scoped_lock lock(c.mutex);
				if (SOCKET sd = c.sock; sd != OPENMSX_INVALID_SOCKET) {
					sock_close(sd);
					c.sock = OPENMSX_INVALID_SOCKET;
				}
				c.pendingClose = false;
				continue;
			}
			SOCKET sd = c.sock;
			if (sd == OPENMSX_INVALID_SOCKET) continue;
			if (c.tcpState == TcpState::SynSent) {
				// connect() completion shows up as writable (POSIX) or in
				// the exception set (Windows).
				FD_SET(sd, &wfds);
				FD_SET(sd, &efds);
			} else {
				FD_SET(sd, &rfds);
				bool pendingSend;
				{
					std::scoped_lock lock(c.mutex);
					pendingSend = !c.sendBuf.empty();
				}
				if (pendingSend) FD_SET(sd, &wfds);
			}
			maxSock = std::max(maxSock, sd);
		}
		for (auto& u : udp) {
			if (u.pendingClose) {
				std::scoped_lock lock(u.mutex);
				if (SOCKET sd = u.sock; sd != OPENMSX_INVALID_SOCKET) {
					sock_close(sd);
					u.sock = OPENMSX_INVALID_SOCKET;
				}
				u.pendingClose = false;
				continue;
			}
			SOCKET sd = u.sock;
			if (sd == OPENMSX_INVALID_SOCKET) continue;
			FD_SET(sd, &rfds);
			maxSock = std::max(maxSock, sd);
		}
		if (maxSock == 0) { // nothing open (fd 0 is never a socket here)
			// Avoid a tight loop: select() needs at least one fd.
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}
		struct timeval tv = {0, 100000}; // 100 ms
		if (select(static_cast<int>(maxSock) + 1, &rfds, &wfds, &efds, &tv) <= 0) {
			continue; // timeout or error: re-check running and rebuild the set
		}

		for (auto& c : tcp) {
			SOCKET sd = c.sock;
			if (sd == OPENMSX_INVALID_SOCKET) continue;

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
						if (!c.pendingClose) c.tcpState = TcpState::Established;
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

				std::scoped_lock lock(c.mutex);
				// The MSX may have closed the listener while we were accepting,
				// and the remote-IP filter has to be read coherently.
				if (c.pendingClose || c.tcpState != TcpState::Listen ||
				    (c.remoteIp != 0 && peerIp != c.remoteIp)) {
					sock_close(a);
					continue;
				}
				// Replace the listening socket with the accepted one, publishing
				// the state and the address together.
				sock_close(sd);
				c.sock       = a;
				c.tcpState   = TcpState::Established;
				c.remoteIp   = peerIp;
				c.remotePort = peerPort;
				continue;
			}

			if (c.tcpState != one_of(TcpState::Established, TcpState::CloseWait)) {
				continue;
			}

			// Push out whatever the MSX queued via TCP_SEND. sock_send() is
			// non-blocking, so doing it under the lock is fine; a full kernel
			// buffer (n == 0) just means 'try again on the next pass'.
			if (FD_ISSET(sd, &wfds)) {
				bool sendFailed = false;
				{
					std::scoped_lock lock(c.mutex);
					while (!c.sendBuf.empty()) {
						auto n = sock_send(sd,
							reinterpret_cast<const char*>(c.sendBuf.data()),
							c.sendBuf.size());
						if (n < 0) { sendFailed = true; break; }
						if (n == 0) break; // would block
						c.sendBuf.erase(c.sendBuf.begin(), c.sendBuf.begin() + n);
					}
				}
				if (sendFailed) {
					requestClose(c, CloseReason::ConnectionReset);
					continue;
				}
				// A TCP_CLOSE that arrived with data still queued: now that the
				// data is out, it is safe to send the FIN.
				std::scoped_lock lock(c.mutex);
				if (c.pendingShutdown && c.sendBuf.empty()) {
					shutdownSend(sd);
					c.pendingShutdown = false;
				}
			}

			// Incoming data.
			if (!FD_ISSET(sd, &rfds)) continue;
			std::array<char, 512> buf;
			auto n = sock_recv(sd, buf.data(), buf.size());
			if (n > 0) {
				std::scoped_lock lock(c.mutex);
				if (c.pendingClose) continue;
				size_t room = MAX_RECV_BUF - std::min(MAX_RECV_BUF, c.recvBuf.size());
				auto count = std::min(static_cast<size_t>(n), room);
				const auto* d = reinterpret_cast<const uint8_t*>(buf.data());
				c.recvBuf.insert(c.recvBuf.end(), d, d + count);
			} else if (n == 0) {
				std::scoped_lock lock(c.mutex);
				if (!c.pendingClose) c.tcpState = TcpState::CloseWait;
			} else {
				requestClose(c, CloseReason::ConnectionReset);
			}
		}

		for (auto& u : udp) {
			SOCKET sd = u.sock;
			if (sd == OPENMSX_INVALID_SOCKET) continue;
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
			if (u.pendingClose) continue;
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

	// Async resolution
	if (dns.status == DnsStatus::InProgress) {
		// A query is already in progress
		setError();
		return;
	}

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
			.ip     = dns.resolvedIp});
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
		c.pendingShutdown = false;
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
	if (c.sock == OPENMSX_INVALID_SOCKET || c.pendingClose ||
		c.tcpState != one_of(TcpState::Established, TcpState::CloseWait)) {
		setResultByte(1);
		return;
	}

	if (paramBuf.size() < sizeof(TcpSendParamHeader) + len) {
		setResultByte(1);
		return;
	}

	// Hand the data to the receiver thread rather than sending it here:
	// a full kernel send buffer would otherwise block the emulation
	// thread until the peer drains it, which can take seconds.
	const auto* data = paramBuf.data() + sizeof(TcpSendParamHeader);
	{
		std::scoped_lock lock(c.mutex);
		if (c.sendBuf.size() + len > MAX_SEND_BUF) {
			setResultByte(1); // buffer full: the MSX should retry later
			return;
		}
		c.sendBuf.insert(c.sendBuf.end(), data, data + len);
	}

	setResultByte(0); // accepted
}

// TCP_RECV (0x05)
// Params: handle[1] + maxlen[2 LE]
// Result: actual_len[2 LE] + data[actual_len]

void UnapiNet::cmdTcpRecv()
{
	if (paramBuf.size() < sizeof(TcpRecvParams)) {
		setResult(TcpRecvResultHeader{}, std::span<const uint8_t>{}); // 0 bytes
		return;
	}

	auto p = fromBytes<TcpRecvParams>(paramBuf);
	int h = p.handle;
	uint16_t maxlen = p.maxlen;

	auto* cp = tcpForHandle(h);
	if (!cp) {
		setResult(TcpRecvResultHeader{}, std::span<const uint8_t>{});
		return;
	}
	auto& c = *cp;

	// Clamp to the maximum transfer size
	if (maxlen > MAX_TRANSFER) maxlen = static_cast<uint16_t>(MAX_TRANSFER);

	std::vector<uint8_t> payload;
	{
		std::scoped_lock lock(c.mutex);
		size_t avail = std::min(static_cast<size_t>(maxlen), c.recvBuf.size());
		payload.assign(c.recvBuf.begin(), c.recvBuf.begin() + avail);
		c.recvBuf.erase(c.recvBuf.begin(), c.recvBuf.begin() + avail);
	}

	TcpRecvResultHeader hdr{.actualLen = uint16_t(payload.size())};
	setResult(hdr, payload);
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
		// Close all transient connections
		for (auto& c : tcp) {
			if (!c.resident && c.sock != OPENMSX_INVALID_SOCKET && !c.pendingClose) {
				requestClose(c, CloseReason::ClosedByUser, true);
			}
		}
		setResultByte(0);
		return;
	}

	auto* cp = tcpForHandle(h);
	if (!cp) {
		setResultByte(1);
		return;
	}
	auto& c = *cp;
	{
		std::scoped_lock lock(c.mutex);
		SOCKET sd = c.sock;
		if (sd == OPENMSX_INVALID_SOCKET || c.pendingClose) {
			setResultByte(1);
			return;
		}
		// Graceful shutdown: send FIN and let the receiver thread notice the
		// remote close. (Closing the socket here would race with the receiver,
		// which may be inside select()/recv() on this very fd.)
		// If the MSX still has data queued, the FIN has to wait for it: the
		// receiver sends it once sendBuf has drained.
		if (c.sendBuf.empty()) {
			shutdownSend(sd);
		} else {
			c.pendingShutdown = true;
		}
		c.closeReason = CloseReason::ClosedByUser;
		c.tcpState = TcpState::CloseWait;
	}

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
	if (!cp || cp->sock == OPENMSX_INVALID_SOCKET || cp->pendingClose) {
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
	u.pendingClose = false;
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
			if (!u.resident && u.sock != OPENMSX_INVALID_SOCKET && !u.pendingClose) {
				requestClose(u);
			}
		}
		setResultByte(0);
		return;
	}

	auto* up = udpForHandle(h);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET || up->pendingClose) {
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
		if (auto* up = udpForHandle(h);
		    up && up->sock != OPENMSX_INVALID_SOCKET && !up->pendingClose) {
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
	if (!up || up->sock == OPENMSX_INVALID_SOCKET || up->pendingClose) {
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
	int n = sendto(static_cast<SOCKET>(u.sock), data, len, 0,
				   reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));
	setResultByte(n == len ? 0 : 1);
}

// UDP_RECV (0x0F)
// Params: handle[1] + maxlen[2 LE]
// Result: src_IP[4] + src_port[2 LE] + actual_len[2 LE] + data

void UnapiNet::cmdUdpRecv()
{
	if (paramBuf.size() < sizeof(UdpRecvParams)) {
		setResult(UdpRecvResultHeader{}, std::span<const uint8_t>{});
		return;
	}
	auto p = fromBytes<UdpRecvParams>(paramBuf);
	int h = p.handle;
	uint16_t maxlen = p.maxlen;

	auto* up = udpForHandle(h);
	if (!up || up->sock == OPENMSX_INVALID_SOCKET || up->pendingClose) {
		setResult(UdpRecvResultHeader{}, std::span<const uint8_t>{});
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

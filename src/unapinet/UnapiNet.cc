// Include Socket.hh BEFORE our own header so that SOCKET is defined.
// We use a trick: undefine the "interface" macro that windows.h defines
// and which breaks other openMSX headers.
#include "Socket.hh"
#ifdef _WIN32
#include <ws2tcpip.h>
#ifdef interface
#undef interface
#endif
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#endif

#include "UnapiNet.hh"
#include "serialize.hh"

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

namespace openmsx {

// Constructor / Destructor

UnapiNet::UnapiNet(const DeviceConfig& config)
	: MSXDevice(config)
	, state(State::IDLE)
	, statusReg(STATUS_OK)
	, resultPos(0)
{
	paramBuf.reserve(MAX_TRANSFER + 16);
	resultBuf.reserve(MAX_TRANSFER + 16);

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
	state     = State::IDLE;
	statusReg = STATUS_OK;
	paramBuf.clear();
	resultBuf.clear();
	resultPos = 0;

	closeAllConnections();

	dns.status = DnsStatus::Idle;
	dns.resolvedIp = 0;
	dns.errorCode = 0;
}

// Port reads

byte UnapiNet::readIO(uint16_t port, EmuTime /*time*/)
{
	switch (port & 0xFF) {

	case 0x28: // status register
		return statusReg;

	case 0x29: // data register
		if (state == State::RESULT_READY && resultPos < resultBuf.size()) {
			uint8_t b = resultBuf[resultPos++];
			if (resultPos >= resultBuf.size()) {
				state     = State::IDLE;
				statusReg = STATUS_OK;
			}
			return b;
		}
		return 0x00;

	default:
		return 0xFF;
	}
}

// Port writes

void UnapiNet::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	switch (port & 0xFF) {

	case 0x28: // command
		processCmd(value);
		break;

	case 0x29: // parameter (accumulate)
		// If there is a pending unread result, discard it
		// so that the new parameters are accepted
		if (state == State::RESULT_READY) {
			state     = State::IDLE;
			statusReg = STATUS_OK;
			resultBuf.clear();
			resultPos = 0;
		}
		paramBuf.push_back(value);
		break;
	}
}

// Result helpers

void UnapiNet::setResult(const uint8_t* data, size_t len)
{
	resultBuf.assign(data, data + len);
	resultPos = 0;
	state     = State::RESULT_READY;
	statusReg = STATUS_DATA;
}

void UnapiNet::setResultByte(uint8_t b)
{
	setResult(&b, 1);
}

void UnapiNet::setError()
{
	resultBuf.clear();
	resultPos = 0;
	state     = State::IDLE;
	statusReg = STATUS_ERROR;
}

// TCP handle management

int UnapiNet::allocTcpHandle()
{
	for (int i = 0; i < MAX_TCP; i++) {
		if (tcp[i].sock == OPENMSX_INVALID_SOCKET &&
			tcp[i].tcpState == TCP_CLOSED) {
			return i + 1; // handles 1-based
		}
	}
	return 0; // no free handles
}

UnapiNet::TcpConnection* UnapiNet::tcpForHandle(int h)
{
	return (h >= 1 && h <= MAX_TCP) ? &tcp[h - 1] : nullptr;
}

UnapiNet::UdpConnection* UnapiNet::udpForHandle(int h)
{
	return (h >= 1 && h <= MAX_UDP) ? &udp[h - 1] : nullptr;
}

void UnapiNet::closeTcpSocket(int h)
{
	auto* cp = tcpForHandle(h);
	if (!cp) return;
	auto& c = *cp;
	if (c.sock != OPENMSX_INVALID_SOCKET) {
		sock_close(static_cast<SOCKET>(c.sock));
		c.sock = OPENMSX_INVALID_SOCKET;
	}
	c.tcpState   = TCP_CLOSED;
	c.connecting  = false;
	c.remoteIp    = 0;
	c.remotePort  = 0;
	c.localPort   = 0;
	c.resident    = false;
	{
		std::scoped_lock lock(c.mutex);
		c.recvBuf.clear();
	}
}

void UnapiNet::closeAllConnections()
{
	for (int i = 1; i <= MAX_TCP; i++) {
		closeTcpSocket(i);
		tcp[i - 1].closeReason = CloseReason::NeverUsed;
	}
	for (int i = 1; i <= MAX_UDP; i++) {
		closeUdpSocket(i);
	}
}

// Network receiver thread (background)
//
// Polls all active TCP sockets and moves incoming data into each
// connection's recvBuf. Also detects completion of a non-blocking
// connect() and state transitions (remote close, etc.).

void UnapiNet::forceClose(TcpConnection& c, CloseReason reason)
{
	c.closeReason = reason;
	if (c.sock != OPENMSX_INVALID_SOCKET) {
		sock_close(c.sock);
		c.sock = OPENMSX_INVALID_SOCKET;
	}
	c.tcpState   = TCP_CLOSED;
	c.connecting = false;
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
		bool any = false;
		for (auto& c : tcp) {
			if (c.sock == OPENMSX_INVALID_SOCKET) continue;
			if (c.connecting) {
				// connect() completion shows up as writable (POSIX) or in
				// the exception set (Windows).
				FD_SET(c.sock, &wfds);
				FD_SET(c.sock, &efds);
			} else {
				FD_SET(c.sock, &rfds);
			}
			maxSock = std::max(maxSock, c.sock);
			any = true;
		}
		for (auto& u : udp) {
			if (u.sock == OPENMSX_INVALID_SOCKET) continue;
			FD_SET(u.sock, &rfds);
			maxSock = std::max(maxSock, u.sock);
			any = true;
		}
		if (!any) {
			// Nothing open yet: avoid a tight loop (select() needs >=1 fd).
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}
		struct timeval tv = {0, 100000}; // 100 ms
		if (select(static_cast<int>(maxSock) + 1, &rfds, &wfds, &efds, &tv) <= 0) {
			continue; // timeout or error: re-check running and rebuild the set
		}

		for (auto& c : tcp) {
			if (c.sock == OPENMSX_INVALID_SOCKET) continue;
			SOCKET sd = c.sock;

			// Pending connect() completion.
			if (c.connecting) {
				if (FD_ISSET(sd, &efds)) {
					forceClose(c, CloseReason::ConnectFailed);
				} else if (FD_ISSET(sd, &wfds)) {
					int err = 0;
					::socklen_t elen = sizeof(err);
					getsockopt(sd, SOL_SOCKET, SO_ERROR,
					           reinterpret_cast<char*>(&err), &elen);
					if (err == 0) {
						c.tcpState   = TCP_ESTABLISHED;
						c.connecting = false;
					} else {
						forceClose(c, CloseReason::ConnectFailed);
					}
				}
				continue;
			}

			if (!FD_ISSET(sd, &rfds)) continue;

			// Listening socket: accept the pending connection.
			if (c.tcpState == TCP_LISTEN) {
				struct sockaddr_in peer;
				::socklen_t plen = sizeof(peer);
				SOCKET a = accept(sd, reinterpret_cast<struct sockaddr*>(&peer), &plen);
				if (a == OPENMSX_INVALID_SOCKET) continue;
				uint32_t peerIp = ntohl(peer.sin_addr.s_addr);
				// If a specific remote IP was requested, reject others.
				if (c.remoteIp != 0 && peerIp != c.remoteIp) {
					sock_close(a);
					continue;
				}
				// Replace the listening socket with the accepted one.
				sock_close(sd);
				sock_setNonBlocking(a);
				sock_setIntOption(a, IPPROTO_TCP, TCP_NODELAY);
				c.sock = a;
				c.tcpState = TCP_ESTABLISHED;
				c.remoteIp = peerIp;
				c.remotePort = ntohs(peer.sin_port);
				continue;
			}

			// Incoming data (ESTABLISHED or CLOSE_WAIT).
			if (c.tcpState != TCP_ESTABLISHED &&
			    c.tcpState != TCP_CLOSE_WAIT) {
				continue;
			}
			char buf[512];
			auto n = sock_recv(sd, buf, sizeof(buf));
			if (n > 0) {
				std::scoped_lock lock(c.mutex);
				for (ptrdiff_t j = 0; j < n; j++) {
					if (c.recvBuf.size() < MAX_RECV_BUF) {
						c.recvBuf.push_back(static_cast<uint8_t>(buf[j]));
					}
				}
			} else if (n == 0) {
				c.tcpState = TCP_CLOSE_WAIT;
			} else {
				forceClose(c, CloseReason::ConnectionReset);
			}
		}

		for (auto& u : udp) {
			if (u.sock == OPENMSX_INVALID_SOCKET) continue;
			if (!FD_ISSET(u.sock, &rfds)) continue;
			SOCKET sd = u.sock;
			char buf[2048];
			struct sockaddr_in src;
			::socklen_t slen = sizeof(src);
			int n = recvfrom(sd, buf, sizeof(buf), 0,
			                 reinterpret_cast<struct sockaddr*>(&src), &slen);
			if (n <= 0) continue;
			UdpDatagram dg;
			dg.srcIp = ntohl(src.sin_addr.s_addr);
			dg.srcPort = ntohs(src.sin_port);
			dg.data.assign(buf, buf + n);
			std::scoped_lock lock(u.mutex);
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
	QueryCapResult r{};
	r.cap0 = CAP_BYTE0;
	r.cap1 = CAP_BYTE1;
	setResult(r);
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
	if (paramBuf.empty()) {
		setError();
		return;
	}

	// Extract hostname (null-terminated)
	std::string hostname(paramBuf.begin(), paramBuf.end());
	// Ensure termination
	auto pos = hostname.find('\0');
	if (pos != std::string::npos) {
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
		dns.status = DnsStatus::Complete; // complete
		dns.errorCode = 0;

		DnsQueryResult r{};
		r.status = 1; // resolved immediately
		r.ip     = ip;
		setResult(r);
		return;
	}

	// Async resolution
	if (dns.status == DnsStatus::InProgress) {
		// A query is already in progress
		setError();
		return;
	}

	dns.status = DnsStatus::InProgress; // in_progress
	dns.resolvedIp = 0;
	dns.errorCode = 0;

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
			dns.errorCode = 0;
			dns.status = DnsStatus::Complete; // complete
			freeaddrinfo(res);
		} else {
			if (res) freeaddrinfo(res);
			dns.errorCode = 3; // host name does not exist
			dns.status = DnsStatus::Error; // error
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
	auto s = dns.status.load();

	if (s == DnsStatus::Complete) {
		// Complete
		DnsStatusResult r{};
		r.status = 2;
		r.ip     = dns.resolvedIp;
		setResult(r);
	} else if (s == DnsStatus::Error) {
		// Error
		DnsStatusError r{};
		r.status    = 0xFF;
		r.errorCode = dns.errorCode;
		setResult(r);
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
	if (h == 0) {
		setResultByte(0);
		return;
	}

	auto& c = tcp[h - 1];

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == OPENMSX_INVALID_SOCKET) {
		setResultByte(0);
		return;
	}

	sock_setIntOption(s, IPPROTO_TCP, TCP_NODELAY);
	sock_setNonBlocking(s);

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
		c.sock       = s;
		c.tcpState   = TCP_LISTEN;
		c.connecting = false;

		// Read back actual local port
		::socklen_t alen = sizeof(addr);
		if (getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &alen) == 0) {
			c.localPort = ntohs(addr.sin_port);
		} else {
			c.localPort = localPortReq;
		}
		c.remoteIp   = ip;   // 0 = any; otherwise filter in receiverLoop
		c.remotePort = remotePort;
	} else {
		// Active connect
		sockaddr_in dest = sock_makeIPv4(ip, remotePort);
		int ret = connect(s, reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));
		if (ret == 0) {
			c.sock       = s;
			c.tcpState   = TCP_ESTABLISHED;
			c.connecting = false;
		} else {
#ifdef _WIN32
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
#else
			if (errno == EINPROGRESS) {
#endif
				c.sock       = s;
				c.tcpState   = TCP_SYN_SENT;
				c.connecting = true;
			} else {
				sock_close(s);
				setResultByte(0);
				return;
			}
		}
		c.remoteIp   = ip;
		c.remotePort = remotePort;

		// Obtener puerto local asignado
		struct sockaddr_in local;
		::socklen_t len = sizeof(local);
		if (getsockname(s, reinterpret_cast<struct sockaddr*>(&local), &len) == 0) {
			c.localPort = ntohs(local.sin_port);
		}
	}

	c.closeReason = CloseReason::None;
	c.resident    = resident;
	{
		std::scoped_lock lock(c.mutex);
		c.recvBuf.clear();
	}

	setResultByte(static_cast<uint8_t>(h));
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
	if (c.sock == OPENMSX_INVALID_SOCKET ||
		(c.tcpState != TCP_ESTABLISHED && c.tcpState != TCP_CLOSE_WAIT)) {
		setResultByte(1);
		return;
	}

	if (paramBuf.size() < sizeof(TcpSendParamHeader) + len) {
		setResultByte(1);
		return;
	}

	// Send data
	const char* data = reinterpret_cast<const char*>(paramBuf.data() + sizeof(TcpSendParamHeader));
	size_t sent = 0;
	while (sent < len) {
		auto n = sock_send(c.sock, data + sent, len - sent);
		if (n < 0) {
			// Real socket error: drop the connection.
			forceClose(c, CloseReason::ConnectionReset);
			setResultByte(1);
			return;
		}
		if (n == 0) {
			// EWOULDBLOCK (sock_send maps it to 0): the kernel send buffer is
			// full. Wait (bounded) for the socket to drain and retry, instead
			// of wrongly dropping the connection on a transient stall.
			fd_set wfds;
			FD_ZERO(&wfds);
			FD_SET(c.sock, &wfds);
			timeval tv = {2, 0};
			if (select(static_cast<int>(c.sock) + 1, nullptr, &wfds, nullptr, &tv) <= 0) {
				forceClose(c, CloseReason::ConnectionReset);
				setResultByte(1);
				return;
			}
			continue;
		}
		sent += static_cast<size_t>(n);
	}

	setResultByte(0); // OK
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
	payload.reserve(maxlen);
	{
		std::scoped_lock lock(c.mutex);
		uint16_t avail = static_cast<uint16_t>(
			std::min(static_cast<size_t>(maxlen), c.recvBuf.size()));
		for (uint16_t i = 0; i < avail; i++) {
			payload.push_back(c.recvBuf.front());
			c.recvBuf.pop_front();
		}
	}

	TcpRecvResultHeader hdr{};
	hdr.actualLen = static_cast<uint16_t>(payload.size());
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
		for (int i = 0; i < MAX_TCP; i++) {
			if (!tcp[i].resident && tcp[i].sock != OPENMSX_INVALID_SOCKET) {
				tcp[i].closeReason = CloseReason::ClosedByUser;
				closeTcpSocket(i + 1);
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
	if (c.sock == OPENMSX_INVALID_SOCKET) {
		setResultByte(1);
		return;
	}

	// Graceful shutdown: just FIN; let recvLoop detect remote close
	// and do the actual socket cleanup. Calling sock_close here can
	// deadlock with the recv thread on Windows.
#ifdef _WIN32
	shutdown(c.sock, SD_SEND);
#else
	shutdown(c.sock, SHUT_WR);
#endif
	c.closeReason = CloseReason::ClosedByUser;
	c.tcpState = TCP_CLOSE_WAIT;

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
			uint16_t avail;
			{
				std::scoped_lock lock(c.mutex);
				avail = static_cast<uint16_t>(
					std::min(c.recvBuf.size(), static_cast<size_t>(0xFFFF)));
			}
			r.state       = c.tcpState;
			r.avail       = avail;
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

	cp->closeReason = CloseReason::Aborted;
	closeTcpSocket(h);
	setResultByte(0);
}

// GET_LOCALIP (0x0D)
// Params: -
// Result: 4 bytes IP (big-endian: a.b.c.d)

void UnapiNet::cmdGetLocalIP()
{
	GetLocalIpResult r{};
	r.ip = sock_localIPv4();
	setResult(r);
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
			return i + 1;
		}
	}
	return 0;
}

void UnapiNet::closeUdpSocket(int h)
{
	auto* up = udpForHandle(h);
	if (!up) return;
	auto& u = *up;
	if (u.sock != OPENMSX_INVALID_SOCKET) {
		sock_close(static_cast<SOCKET>(u.sock));
		u.sock = OPENMSX_INVALID_SOCKET;
	}
	u.localPort = 0;
	u.resident = false;
	{
		std::scoped_lock lock(u.mutex);
		u.recvQueue.clear();
	}
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
	if (h == 0) {
		setResultByte(0);
		return;
	}
	auto& u = udp[h - 1];

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

	// Read back actual local port
	::socklen_t alen = sizeof(addr);
	if (getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &alen) == 0) {
		u.localPort = ntohs(addr.sin_port);
	} else {
		u.localPort = localPort;
	}

	u.sock = s;
	u.resident = false;
	{
		std::scoped_lock lock(u.mutex);
		u.recvQueue.clear();
	}

	setResultByte(static_cast<uint8_t>(h));
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
		for (int i = 0; i < MAX_UDP; i++) {
			if (!udp[i].resident && udp[i].sock != OPENMSX_INVALID_SOCKET) {
				closeUdpSocket(i + 1);
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
	closeUdpSocket(h);
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
	UdpStateResult r{};
	r.firstDgramSize = size;
	setResult(r);
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
	if (!up || up->sock == OPENMSX_INVALID_SOCKET) {
		setResult(UdpRecvResultHeader{}, std::span<const uint8_t>{});
		return;
	}
	auto& u = *up;

	UdpDatagram dg;
	bool haveDg = false;
	{
		std::scoped_lock lock(u.mutex);
		if (!u.recvQueue.empty()) {
			dg = std::move(u.recvQueue.front());
			u.recvQueue.pop_front();
			haveDg = true;
		}
	}

	if (!haveDg) {
		setResult(UdpRecvResultHeader{}, std::span<const uint8_t>{});
		return;
	}

	uint16_t actual = static_cast<uint16_t>(
		std::min(static_cast<size_t>(maxlen), dg.data.size()));

	UdpRecvResultHeader hdr{};
	hdr.srcIp     = dg.srcIp;
	hdr.srcPort   = dg.srcPort;
	hdr.actualLen = actual;
	setResult(hdr, std::span<const uint8_t>(dg.data.data(), actual));
}

// ICMP Echo (ping)
//
// Uses the Windows IcmpSendEcho API (doesn't require admin).
// A worker thread handles the (blocking) call and pushes replies
// to a queue that the MSX polls via RCV_ECHO.

#ifdef _WIN32
// Minimal declarations to avoid pulling iphlpapi.h / icmpapi.h
// (which trigger the "interface" macro conflict in windows.h).
extern "C" {
	struct IcmpIpOptions {
		unsigned char Ttl;
		unsigned char Tos;
		unsigned char Flags;
		unsigned char OptionsSize;
		unsigned char* OptionsData;
	};
	struct IcmpEchoReply {
		unsigned long  Address;
		unsigned long  Status;
		unsigned long  RoundTripTime;
		unsigned short DataSize;
		unsigned short Reserved;
		void*          Data;
		IcmpIpOptions  Options;
	};
	__declspec(dllimport) void* __stdcall IcmpCreateFile(void);
	__declspec(dllimport) int   __stdcall IcmpCloseHandle(void*);
	__declspec(dllimport) unsigned long __stdcall IcmpSendEcho(
		void* h, unsigned long addr,
		void* data, unsigned short size,
		IcmpIpOptions* opts,
		void* reply, unsigned long replySize,
		unsigned long timeout);
}
#pragma comment(lib, "iphlpapi.lib")
#endif

void UnapiNet::icmpWorkerLoop()
{
#ifdef _WIN32
	void* hIcmp = IcmpCreateFile();
	if (!hIcmp || hIcmp == reinterpret_cast<void*>(-1)) return;

	while (running) {
		if (!icmpPending.exchange(false)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}

		IcmpRequest req = icmpRequest;

		std::vector<uint8_t> payload(req.dataLen);
		for (size_t i = 0; i < payload.size(); i++)
			payload[i] = static_cast<uint8_t>(i);

		unsigned long replySize = sizeof(IcmpEchoReply) + req.dataLen + 8;
		std::vector<uint8_t> replyBuf(replySize);

		IcmpIpOptions opt = {};
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
			auto* reply = reinterpret_cast<IcmpEchoReply*>(replyBuf.data());
			if (reply->Status == 0 /* IP_SUCCESS */) {
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
	IcmpReply r;
	bool have;
	{
		std::scoped_lock lock(icmpMutex);
		have = !icmpReplies.empty();
		if (have) {
			r = icmpReplies.front();
			icmpReplies.pop_front();
		}
	}

	if (!have) {
		setResultByte(0); // status 0 = no data
		return;
	}

	IcmpRecvResult out{};
	out.hasData    = 1;
	out.srcIp      = r.srcIp;
	out.ttl        = r.ttl;
	out.identifier = r.identifier;
	out.sequence   = r.sequence;
	out.dataLen    = r.dataLen;
	setResult(out);
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

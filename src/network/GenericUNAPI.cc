#include "GenericUNAPI.hh"

#include "MSXCliComm.hh"
#include "MSXException.hh"
#include "OpenSSL.hh"
#include "Poller.hh"
#include "serialize.hh"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string_view>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif

namespace openmsx {

// ---- helper: check if a custom command is "quick" (no parameters) ----
// Mirrors the QUICK_COMMAND set executed directly in the RX_PARSER_IDLE
// state of the ESP32 UNAPI firmware (see received_data_parser()).
static bool isQuickCustomCommand(uint8_t cmd)
{
	switch (cmd) {
	case 'R': case 'W': case '?': case 'V': case 'r':
	case 'S': case 's': case 'g': case 'N': case 'D':
	case 'O': case 'o': case 'Q': case 'c': case 'G':
	case 'H': case 'h': case 'a': case 'w': case 'E': case 'I':
		return true;
	default:
		return false;
	}
}

// ---- helper: check if a command carries a 2-byte size header + data ----
// These commands move the parser to RX_PARSER_WAIT_DATA_SIZE. Mirrors the
// command list of the ESP32 firmware's received_data_parser(); any other
// byte is discarded while the parser stays IDLE.
static bool isDataCommand(uint8_t cmd)
{
	switch (cmd) {
	case 1: case 2: case 3: case 6:            // GET_CAPAB, GET_IPINFO, NET_STATE, DNS_Q
	case 8: case 9: case 10: case 11: case 12: // UDP OPEN/CLOSE/STATE/SEND/RCV
	case 13: case 14: case 15: case 16:        // TCP OPEN/CLOSE/ABORT/STATE
	case 17: case 18:                          // TCP SEND/RCV
	case 25: case 26:                          // CONFIG_AUTOIP, CONFIG_IP
	case 206:                                  // TCPIP_DNS_Q_NEW
		return true;
	}
	if (cmd >= 129 && cmd <= 143) return true; // SSH UNAPI
	if (cmd >= 200 && cmd <= 202) return true; // HTTP client
	switch (cmd) {
	case 'A': case 'B': case 'd': case 'U': case 'u':
	case 'Z': case 'Y': case 'z': case 'T': case 'C':
		return true;
	default:
		return false;
	}
}

// Max command data block size (MAX_CMD_DATA_LEN in UNAPIESP.h)
static constexpr unsigned MAX_CMD_DATA_LEN = 2148;

// Boot greeting (the device identifies itself as a generic openMSX
// TCP/IP UNAPI adapter)
static constexpr std::string_view bootGreeting =
	"TCP-IP UNAPI openMSX Generic v1.0\r\n";

// ---- baud-rate table (kept for compatibility with SMXWiFi I/O) ----
static constexpr unsigned baudRates[] = {
	859372, 346520, 231014, 115200, 57600, 38400, 31250, 19200, 9600, 4800
};

// ---- connection structure ----
// ---- helper: set a socket non-blocking (accepted sockets don't inherit
// the mode on POSIX) ----
static void setNonBlocking(SOCKET s)
{
#ifdef _WIN32
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
#else
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

// Passive TCP listener registry entry: multiple passive connections may
// share a listener for the same local port; the socket is owned by the
// registry and closed when the last referencing connection is freed.
struct GenericUNAPI::ListenSocket {
	SOCKET sock = OPENMSX_INVALID_SOCKET;
	uint16_t port = 0;
	int refs = 0;
};

struct GenericUNAPI::Connection {
	int type = 0; // 0=free, 1=TCP, 2=UDP
	SOCKET sock = OPENMSX_INVALID_SOCKET;

	// Passive TCP: the (possibly shared) listener socket and the accepted
	// client socket. clientSock stays open after the remote side closes so
	// that SEND/RCV/STATE keep reporting a connected (dead) connection,
	// like the firmware's ClientList.
	SOCKET listenSock = OPENMSX_INVALID_SOCKET;
	SOCKET clientSock = OPENMSX_INVALID_SOCKET;
	int listenIdx = -1; // index into the listenSockets registry
	std::atomic<bool> clientEof{false}; // accepted client closed remotely

	uint32_t remoteIP = 0;
	uint16_t remotePort = 0;
	uint16_t localPort = 0;
	uint8_t flags = 0;
	std::atomic<uint8_t> state{0}; // 0=closed, 1=opening, 2=open, 3=closing

	// TLS (TCP active connections, TCP-IP UNAPI spec 1.1): opaque OpenSSL
	// session. The handshake runs in the reader thread; the connection is
	// only considered ESTABLISHED once it has completed (spec: TCP_STATE
	// must not report ESTABLISHED before the handshake is finished).
	void* ssl = nullptr;        // OpenSSL session (null when not using TLS)
	bool tlsVerify = false;     // validate the server certificate
	std::atomic<uint8_t> handshakePhase{0}; // 0=no TLS, 1=handshaking, 2=done

	cb_queue<uint8_t> recvBuffer;
	mutable std::mutex recvMutex;

	std::unique_ptr<std::thread> readerThread;
	std::unique_ptr<Poller> poller;
	std::atomic<bool> readerActive{false};
};

// ====================================================================
//  Construction / destruction
// ====================================================================

GenericUNAPI::GenericUNAPI(DeviceConfig& config)
	: MSXDevice(config)
	, enabledSetting(
		getCommandController(), "genericunapi-enabled",
		"Enable GenericUNAPI network device", true)
	, localIpSetting(
		getCommandController(), "genericunapi-local-ip",
		"Local IP address reported to MSX", "0.0.0.0")
	, gatewaySetting(
		getCommandController(), "genericunapi-gateway",
		"Default gateway reported to MSX", "0.0.0.0")
	, subnetSetting(
		getCommandController(), "genericunapi-subnet",
		"Subnet mask reported to MSX", "0.0.0.0")
	, dnsPrimarySetting(
		getCommandController(), "genericunapi-dns-primary",
		"Primary DNS server reported to MSX", "8.8.8.8")
	, dnsSecondarySetting(
		getCommandController(), "genericunapi-dns-secondary",
		"Secondary DNS server reported to MSX", "0.0.0.0")
{
	// Probe for a host-installed OpenSSL runtime; TLS support is
	// advertised and enabled only when one is found (the TLS capability
	// bits are not advertised otherwise, and TLS TCP_OPEN requests get
	// ERR_NOT_IMP).
	if (OpenSSL::load()) {
		getCliComm().printInfo(
			"GenericUNAPI: TLS support enabled (",
			OpenSSL::version(), ")");
	} else {
		getCliComm().printInfo(
			"GenericUNAPI: OpenSSL not found, TLS support disabled "
			"(install OpenSSL to enable it)");
	}

	// Start the ESP emulation thread immediately.  Note that we can NOT
	// rely on powerUp() to do this: when the extension is inserted into an
	// already-running machine, powerUp() is never called on the new device.
	startEspThread();
}

GenericUNAPI::~GenericUNAPI()
{
	stopEspThread();
}

// ====================================================================
//  Power / reset
// ====================================================================

void GenericUNAPI::powerUp(EmuTime time)
{
	reset(time);
}

void GenericUNAPI::powerDown(EmuTime /*time*/)
{
	stopEspThread();
}

void GenericUNAPI::reset(EmuTime /*time*/)
{
	// Close all connections and stop reader threads
	{
		std::scoped_lock lock(connectionsMutex);
		for (auto& cp : connections) {
			if (!cp) continue;
			cp->readerActive = false;
			if (cp->poller) cp->poller->abort();
			if (cp->readerThread && cp->readerThread->joinable()) {
				cp->readerThread->join();
			}
			cp->readerThread.reset();
			cp->poller.reset();
			if (cp->ssl) {
				OpenSSL::close(cp->ssl);
				cp->ssl = nullptr;
			}
			if (cp->sock != OPENMSX_INVALID_SOCKET) {
				sock_close(cp->sock);
			}
			cp.reset();
		}
	}
	resetFifo();
	lastResponse.clear();
	startEspThread();
}

// ====================================================================
//  I/O port handlers  (same layout as SMXWiFi)
// ====================================================================

uint8_t GenericUNAPI::readIO(uint16_t port, EmuTime /*time*/)
{
	switch (port & 0x01) {
	case 0:
		return readEspToMsxFifo();
	case 1:
		return readStatus();
	default:
		return 0xFF;
	}
}

uint8_t GenericUNAPI::peekIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x01) {
	case 1: {
		std::scoped_lock lock(espToMsxMutex);
		uint8_t status = 0;
		if (!espToMsxFifo.empty()) status |= 0x01;
		status |= 0x08;
		if (underrun) status |= 0x10;
		return status;
	}
	default:
		return 0xFF;
	}
}

void GenericUNAPI::writeIO(uint16_t port, uint8_t value, EmuTime /*time*/)
{
	switch (port & 0x01) {
	case 0:
		writeCommand(value);
		break;
	case 1: {
		std::scoped_lock lock(msxToEspMutex);
		msxToEspFifo.push_back(value);
		msxToEspCond.notify_one();
		break;
	}
	}
}

// ====================================================================
//  FIFO management
// ====================================================================

uint8_t GenericUNAPI::readEspToMsxFifo()
{
	std::unique_lock lock(espToMsxMutex);
	if (!espToMsxFifo.empty()) {
		uint8_t v = espToMsxFifo.front();
		espToMsxFifo.pop_front();
		return v;
	}
	// Block for a short time waiting for the ESP thread to provide data
	espToMsxCond.wait_for(lock, std::chrono::milliseconds(30));
	if (!espToMsxFifo.empty()) {
		uint8_t v = espToMsxFifo.front();
		espToMsxFifo.pop_front();
		return v;
	}
	underrun = true;
	return 0xFF;
}

uint8_t GenericUNAPI::readStatus()
{
	std::scoped_lock lock(espToMsxMutex);
	uint8_t status = 0;
	if (!espToMsxFifo.empty()) status |= 0x01;
	status |= 0x08; // Quick receive supported
	if (underrun) {
		status |= 0x10;
		underrun = false;
	}
	return status;
}

void GenericUNAPI::writeCommand(uint8_t value)
{
	// Baud-rate selection is a no-op for the emulated device.
	// Only FIFO reset (value==20) is relevant.
	if (value == 20) {
		resetFifo();
	}
}

void GenericUNAPI::resetFifo()
{
	std::scoped_lock lock(espToMsxMutex);
	espToMsxFifo.clear();
	underrun = false;
	{
		std::scoped_lock lock2(msxToEspMutex);
		msxToEspFifo.clear();
	}
}

// ====================================================================
//  Thread helpers
// ====================================================================

void GenericUNAPI::startEspThread()
{
	if (espRunning.load()) return;
	espRunning = true;
	espThread = std::thread([this] { espThreadFunc(); });
}

void GenericUNAPI::stopEspThread()
{
	if (!espRunning.load()) return;
	espRunning = false;
	{
		std::scoped_lock lock(msxToEspMutex);
		msxToEspCond.notify_one();
	}
	if (espThread.joinable()) {
		espThread.join();
	}
	// Stop any reader threads still running
	{
		std::scoped_lock lock(connectionsMutex);
		for (auto& cp : connections) {
			if (!cp) continue;
			cp->readerActive = false;
			if (cp->poller) cp->poller->abort();
			if (cp->readerThread && cp->readerThread->joinable()) {
				cp->readerThread->join();
			}
			cp->readerThread.reset();
			cp->poller.reset();
			if (cp->ssl) {
				OpenSSL::close(cp->ssl);
				cp->ssl = nullptr;
			}
			if (cp->sock != OPENMSX_INVALID_SOCKET) {
				sock_close(cp->sock);
				cp->sock = OPENMSX_INVALID_SOCKET;
			}
		}
	}
}

// ====================================================================
//  ESP emulator thread  (reads MSX→ESP FIFO, processes commands)
// ====================================================================

void GenericUNAPI::espThreadFunc()
{
	// Parser state machine, mirroring received_data_parser() in the
	// ESP32 UNAPI firmware:
	//  - unknown command bytes are discarded and the parser stays IDLE
	//  - known quick commands execute immediately (no size header)
	//  - known data commands expect CMD + 2-byte size (MSB first) + data
	//  - if a partial command is not completed within 250 ms (no new
	//    byte arriving), the parser discards it and reverts to IDLE
	enum class ParserState : uint8_t { IDLE, WAIT_DATA_SIZE, GET_DATA };
	auto state = ParserState::IDLE;
	uint8_t cmdByte = 0;
	uint8_t sizeStep = 0;
	uint16_t expectedSize = 0;
	std::vector<uint8_t> dataBuf;
	auto deadline = std::chrono::steady_clock::now();

	// Boot sequence, mirroring the real device's bootup after 'R':
	// immediate "R0" response, then the greeting string, then
	// "Ready\r\n" every 5 s (up to 3 times). The loop stops as soon as
	// the MSX sends any byte.
	enum class BootStage : uint8_t { NONE, READY_LOOP };
	auto bootStage = BootStage::NONE;
	uint8_t readyRetries = 0;
	auto bootEventTime = std::chrono::steady_clock::now();

	auto pushBootText = [this](std::string_view text) {
		std::scoped_lock lock(espToMsxMutex);
		for (char c : text) {
			espToMsxFifo.push_back(static_cast<uint8_t>(c));
		}
		espToMsxCond.notify_one();
	};

	auto resetParser = [&] {
		state = ParserState::IDLE;
		sizeStep = 0;
		expectedSize = 0;
		dataBuf.clear();
	};

	// Dispatch a complete command. Must be called without msxToEspMutex held.
	auto processCommand = [&] {
		if (cmdByte < 63 || cmdByte >= 0x80) {
			handleUnapiCommand(cmdByte, dataBuf);
		} else {
			handleCustomCommand(cmdByte, dataBuf);
		}
		resetParser();
	};

	while (espRunning.load()) {
		std::unique_lock lock(msxToEspMutex);

		// A reset was requested (cmdReset just ran): discard any pending
		// input — the real device loses its serial buffer on reboot — and
		// start the boot sequence (greeting immediately after "R0").
		if (bootStartPending) {
			bootStartPending = false;
			resetParser();
			msxToEspFifo.clear();
			lock.unlock();
			pushBootText(bootGreeting);
			lock.lock();
			bootStage = BootStage::READY_LOOP;
			readyRetries = 3;
			bootEventTime = std::chrono::steady_clock::now() +
			                std::chrono::seconds(5);
			continue;
		}

		if (msxToEspFifo.empty()) {
			if (state == ParserState::IDLE) {
				if (bootStage == BootStage::NONE) {
					msxToEspCond.wait(lock, [this] {
						return !msxToEspFifo.empty() || !espRunning.load();
					});
					if (!espRunning.load()) break;
				} else {
					// "Ready" loop in progress: wake up on data or at the
					// next "Ready" event.
					bool timedOut = !msxToEspCond.wait_until(
						lock, bootEventTime, [this] {
						return !msxToEspFifo.empty() || !espRunning.load();
					});
					if (!espRunning.load()) break;
					if (timedOut) {
						lock.unlock();
						pushBootText("Ready\r\n");
						lock.lock();
						if (--readyRetries == 0) {
							bootStage = BootStage::NONE;
						}
						bootEventTime = std::chrono::steady_clock::now() +
						                std::chrono::seconds(5);
						continue;
					}
				}
			} else {
				// Waiting for the rest of a framed command: apply the
				// firmware's 250 ms inter-byte timeout.
				bool timedOut = !msxToEspCond.wait_until(
					lock, deadline, [this] {
					return !msxToEspFifo.empty() || !espRunning.load();
				});
				if (!espRunning.load()) break;
				if (timedOut) {
					resetParser();
					continue;
				}
			}
			continue;
		}

		uint8_t b = msxToEspFifo.front();
		msxToEspFifo.pop_front();
		deadline = std::chrono::steady_clock::now() +
		           std::chrono::milliseconds(250);

		switch (state) {
		case ParserState::IDLE:
			// Any byte received during the boot sequence stops it
			bootStage = BootStage::NONE;
			if (isQuickCustomCommand(b)) {
				// Quick custom command, no size header
				lock.unlock();
				handleCustomCommand(b, {});
				continue;
			}
			if (isDataCommand(b)) {
				cmdByte = b;
				state = ParserState::WAIT_DATA_SIZE;
				continue;
			}
			// Unknown command byte: discard, stay IDLE
			continue;

		case ParserState::WAIT_DATA_SIZE:
			// 2-byte size, MSB first
			if (sizeStep == 0) {
				expectedSize = static_cast<uint16_t>(b) << 8;
				sizeStep = 1;
			} else {
				expectedSize |= b;
				if (expectedSize > MAX_CMD_DATA_LEN) {
					resetParser(); // invalid size, discard
				} else if (expectedSize == 0) {
					// Size 0: process immediately, no data block
					lock.unlock();
					processCommand();
				} else {
					dataBuf.clear();
					state = ParserState::GET_DATA;
				}
				sizeStep = 0;
			}
			continue;

		case ParserState::GET_DATA:
			dataBuf.push_back(b);
			if (dataBuf.size() >= expectedSize) {
				lock.unlock();
				processCommand();
			}
			continue;
		}
	}
}

// ====================================================================
//  TCP reader thread  (reads inbound data from a connected TCP socket)
// ====================================================================

void GenericUNAPI::tcpReaderThreadFunc(int connIdx)
{
	std::unique_lock lock(connectionsMutex);
	if (connIdx < 0 || connIdx >= MAX_CONNECTIONS) return;
	auto* conn = connections[connIdx].get();
	if (!conn) return;
	lock.unlock();

	// The sockets are non-blocking. recv()/accept() are gated behind
	// select() so that a "would block" condition can never be mistaken for
	// a close from the remote side (on Windows sock_recv() maps
	// WSAEWOULDBLOCK to -1). Once select() reports a socket readable,
	// recv() only returns data or EOF; EOF sets state=3, which cmdTcpState()
	// reports as lwIP state CLOSE_WAIT, exactly like the real firmware.
	//
	// For passive connections this thread also performs the accept():
	// when the (possibly shared) listener reports a pending connection,
	// it is accepted and attached to this connection if none is attached
	// yet; otherwise it is immediately closed (dropped), like the
	// firmware's accept loop does when a client is already attached.
	//
	// For TLS connections this thread also drives the handshake (SSL_connect
	// needs both read and write readiness); the connection reports
	// SYN-SENT until the handshake and certificate validation have
	// finished (spec 4.5.5). A failed handshake stores the close reason
	// and reports the connection as failed (state=4), so TCP_STATE returns
	// ERR_NO_CONN with the reason, as the spec recommends.
	bool tlsFailed = false;
	auto tlsFail = [&](uint8_t reason) {
		// Stored relaxed and sequenced before the seq_cst state store,
		// so TCP_STATE sees the reason whenever it observes state==4.
		closeReason[connIdx].store(reason, std::memory_order_relaxed);
		conn->state = 4; // failed (TLS) -> TCP_STATE reports ERR_NO_CONN
		tlsFailed = true;
	};
	while (conn->readerActive.load()) {
		fd_set rfds;
		fd_set wfds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		SOCKET s = (conn->listenSock != OPENMSX_INVALID_SOCKET)
		         ? conn->listenSock : conn->sock;
		bool inHandshake = (conn->handshakePhase.load(std::memory_order_relaxed) == 1);
		FD_SET(s, &rfds);
		if (inHandshake) {
			// The TLS handshake also needs to send (ClientHello, ...)
			FD_SET(s, &wfds);
		}
		bool haveClient = (conn->clientSock != OPENMSX_INVALID_SOCKET) &&
		                  !conn->clientEof.load();
		if (haveClient) {
			FD_SET(conn->clientSock, &rfds);
		}
		struct timeval tv = {0, 0}; // poll, never block
#ifdef _WIN32
		int sel = select(0, &rfds,
		                 inHandshake ? &wfds : nullptr, nullptr, &tv);
#else
		int maxFd = static_cast<int>(s);
		if (haveClient) {
			maxFd = std::max(maxFd, static_cast<int>(conn->clientSock));
		}
		int sel = select(maxFd + 1, &rfds,
		                 inHandshake ? &wfds : nullptr, nullptr, &tv);
#endif
		if (sel <= 0) {
			// Nothing to read yet (or transient select error);
			// re-check the shutdown flag and try again
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}
		if (FD_ISSET(s, &rfds) || (inHandshake && FD_ISSET(s, &wfds))) {
			if (conn->listenSock != OPENMSX_INVALID_SOCKET) {
				// Passive: a connection request is pending
				SOCKET ns = accept(conn->listenSock, nullptr, nullptr);
				if (ns != OPENMSX_INVALID_SOCKET) {
					if (conn->clientSock == OPENMSX_INVALID_SOCKET) {
						// No client attached yet: attach this one
						setNonBlocking(ns);
						conn->clientSock = ns;
						conn->clientEof = false;
						struct sockaddr_in peer;
						socklen_t peerLen = sizeof(peer);
						if (getpeername(ns,
						    reinterpret_cast<struct sockaddr*>(&peer),
						    &peerLen) == 0) {
							conn->remoteIP = ntohl(peer.sin_addr.s_addr);
							conn->remotePort = ntohs(peer.sin_port);
						}
						conn->state = 2; // open -> ESTABLISHED
					} else {
						// A client is already attached: drop the
						// pending connection (firmware behavior)
						sock_close(ns);
					}
				}
			} else if (inHandshake) {
				// Active TLS: drive the handshake
				int r = OpenSSL::handshake(conn->ssl);
				if (r == 1) {
					// Handshake finished: validate the certificate
					// before the connection becomes ESTABLISHED
					uint8_t reason = conn->tlsVerify
						? static_cast<uint8_t>(OpenSSL::verifyResult(conn->ssl))
						: 0;
					if (reason != 0) {
						tlsFail(reason);
					} else {
						conn->handshakePhase.store(2, std::memory_order_relaxed);
						conn->state = 2; // open -> ESTABLISHED
					}
				} else if (r < 0) {
					// Handshake failed (protocol error, peer reset, ...):
					// report a TLS error like the firmware (reason 19)
					tlsFail(19);
				}
				// r == 0 or 2: still waiting for read/write readiness
				if (tlsFailed) break;
			} else {
				// Active: read inbound data
				char buf[1024];
				ptrdiff_t n;
				if (conn->ssl) {
					// TLS: decrypt; a zero return is a clean TLS close
					// (close_notify), a -1 with want==0 a TLS error
					int want = 0;
					n = OpenSSL::read(conn->ssl, buf, sizeof(buf), want);
					if (n == 0) {
						conn->state = 3; // remote closed -> CLOSE_WAIT
						break;
					}
					if (n < 0 && want == 0) {
						tlsFail(19); // TLS: other error
						break;
					}
				} else {
					n = sock_recv(conn->sock, buf, sizeof(buf));
					if (n < 0) {
						// Socket closed by the remote side (EOF) or error
						conn->state = 3; // remote closed -> CLOSE_WAIT
						break;
					}
				}
				if (n > 0) {
					std::scoped_lock rlock(conn->recvMutex);
					for (ptrdiff_t i = 0; i < n; ++i) {
						conn->recvBuffer.push_back(static_cast<uint8_t>(buf[i]));
					}
				}
				if (conn->ssl && n >= 0) {
					// The SSL layer may have more plaintext buffered than
					// one SSL_read returns; drain it now (the socket
					// itself would otherwise not be selectable again).
					for (;;) {
						int want = 0;
						n = OpenSSL::read(conn->ssl, buf, sizeof(buf), want);
						if (n == 0) {
							conn->state = 3; // remote closed -> CLOSE_WAIT
							break;
						}
						if (n < 0) {
							if (want == 0) tlsFail(19);
							break;
						}
						std::scoped_lock rlock(conn->recvMutex);
						for (ptrdiff_t i = 0; i < n; ++i) {
							conn->recvBuffer.push_back(static_cast<uint8_t>(buf[i]));
						}
					}
					if (tlsFailed) break;
				}
			}
		}
		if (haveClient && FD_ISSET(conn->clientSock, &rfds)) {
			char buf[1024];
			auto n = sock_recv(conn->clientSock, buf, sizeof(buf));
			if (n < 0) {
				// Accepted client closed the connection: keep it attached
				// (like the firmware's ClientList) and report CLOSE_WAIT;
				// new pending connections keep being dropped.
				conn->clientEof = true;
				conn->state = 3; // remote closed -> CLOSE_WAIT
			} else {
				std::scoped_lock rlock(conn->recvMutex);
				for (ptrdiff_t i = 0; i < n; ++i) {
					conn->recvBuffer.push_back(static_cast<uint8_t>(buf[i]));
				}
			}
		}
	}
}

// ====================================================================
//  Connection manager
// ====================================================================

GenericUNAPI::Connection* GenericUNAPI::allocateConnection()
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (!connections[i] || connections[i]->type == 0) {
			if (!connections[i]) {
				connections[i] = std::make_unique<Connection>();
			} else {
				connections[i]->type = 0;
				connections[i]->sock = OPENMSX_INVALID_SOCKET;
				connections[i]->listenSock = OPENMSX_INVALID_SOCKET;
				connections[i]->clientSock = OPENMSX_INVALID_SOCKET;
				connections[i]->listenIdx = -1;
				connections[i]->clientEof = false;
				connections[i]->recvBuffer.clear();
				connections[i]->remoteIP = 0;
				connections[i]->remotePort = 0;
				connections[i]->localPort = 0;
				connections[i]->flags = 0;
				connections[i]->state = 0;
				connections[i]->ssl = nullptr;
				connections[i]->tlsVerify = false;
				connections[i]->handshakePhase.store(0, std::memory_order_relaxed);
				connections[i]->readerActive = false;
				if (connections[i]->poller) connections[i]->poller->reset();
			}
			closeReason[i].store(0, std::memory_order_relaxed);
			return connections[i].get();
		}
	}
	return nullptr;
}

void GenericUNAPI::releaseListenEntry(int idx)
{
	if (idx < 0 || idx >= static_cast<int>(listenSockets.size())) return;
	auto& e = listenSockets[idx];
	if (--e->refs > 0) return; // still referenced by other connections
	sock_close(e->sock);
	listenSockets.erase(listenSockets.begin() + idx);
}

void GenericUNAPI::freeConnection(int idx)
{
	if (idx < 0 || idx >= MAX_CONNECTIONS) return;
	auto& cp = connections[idx];
	if (!cp) return;
	cp->readerActive = false;
	if (cp->poller) cp->poller->abort();
	if (cp->readerThread && cp->readerThread->joinable()) {
		cp->readerThread->join();
	}
	cp->readerThread.reset();
	cp->poller.reset();
	// Tear down the TLS session first (best-effort close_notify, then
	// free); afterwards the socket can be closed.
	if (cp->ssl) {
		OpenSSL::close(cp->ssl);
		cp->ssl = nullptr;
	}
	if (cp->clientSock != OPENMSX_INVALID_SOCKET) {
		sock_close(cp->clientSock);
		cp->clientSock = OPENMSX_INVALID_SOCKET;
	}
	if (cp->listenIdx >= 0) {
		// Passive connection: the listener socket is shared and owned by
		// the registry; release our reference (the socket is closed when
		// the last referencing connection is freed).
		releaseListenEntry(cp->listenIdx);
		cp->listenIdx = -1;
		cp->listenSock = OPENMSX_INVALID_SOCKET;
		cp->sock = OPENMSX_INVALID_SOCKET; // same shared socket
	} else if (cp->sock != OPENMSX_INVALID_SOCKET) {
		sock_close(cp->sock);
		cp->sock = OPENMSX_INVALID_SOCKET;
	}
	cp->handshakePhase.store(0, std::memory_order_relaxed);
	cp->tlsVerify = false;
	cp->type = 0;
	cp->state = 0;
	cp->clientEof = false;
	cp->recvBuffer.clear();
}

GenericUNAPI::Connection* GenericUNAPI::getConnection(int idx)
{
	if (idx < 0 || idx >= MAX_CONNECTIONS) return nullptr;
	return connections[idx].get();
}

int GenericUNAPI::getFreeConnectionSlot()
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (!connections[i] || connections[i]->type == 0) return i;
	}
	return -1;
}

// ====================================================================
//  Response helpers
// ====================================================================

void GenericUNAPI::saveLastResponse(std::span<const uint8_t> data)
{
	lastResponse.assign(data.begin(), data.end());
}

void GenericUNAPI::sendQuickResponse(uint8_t cmdByte, uint8_t errorCode)
{
	uint8_t resp[2] = {cmdByte, errorCode};
	saveLastResponse({resp, 2});
	{
		std::scoped_lock lock(espToMsxMutex);
		espToMsxFifo.push_back(cmdByte);
		espToMsxFifo.push_back(errorCode);
	}
	espToMsxCond.notify_one();
}

void GenericUNAPI::sendResponse(uint8_t cmdByte, uint8_t errorCode,
                                std::span<const uint8_t> data)
{
	uint16_t respSize = static_cast<uint16_t>(data.size());
	std::vector<uint8_t> resp;
	resp.reserve(4 + respSize);
	resp.push_back(cmdByte);
	resp.push_back(errorCode);
	resp.push_back((respSize >> 8) & 0xFF);
	resp.push_back(respSize & 0xFF);
	resp.insert(resp.end(), data.begin(), data.end());

	saveLastResponse(resp);
	{
		std::scoped_lock lock(espToMsxMutex);
		for (auto b : resp) {
			espToMsxFifo.push_back(b);
		}
	}
	espToMsxCond.notify_one();
}

void GenericUNAPI::sendRawResponse(std::span<const uint8_t> data)
{
	saveLastResponse(data);
	{
		std::scoped_lock lock(espToMsxMutex);
		for (auto b : data) {
			espToMsxFifo.push_back(b);
		}
	}
	espToMsxCond.notify_one();
}

// ====================================================================
//  Command dispatch
// ====================================================================

void GenericUNAPI::handleCustomCommand(uint8_t cmd, std::span<const uint8_t> data)
{
	switch (cmd) {
	case 'R': cmdReset(data); break;
	case 'W': cmdWarmReset(data); break;
	case '?': cmdQuery(data); break;
	case 'V': cmdGetVersion(data); break;
	case 'r': cmdRetry(data); break;
	case 'S': cmdScanAP(data); break;
	case 's': cmdScanResults(data); break;
	case 'A': cmdConnectAP(data); break;
	case 'g': cmdGetAPStatus(data); break;
	case 'B': case 'U': case 'u': case 'Z': case 'Y':
	case 'z': case 'E': case 'I':
		cmdFirmwareUpdate(cmd, data); break;
	case 'N': case 'D': case 'O': case 'o':
	case 'a': case 'H': case 'h':
		sendQuickResponse(cmd, 0); break;
	case 'd': cmdSetBaud(data); break;
	case 'Q': cmdGetSettings(data); break;
	case 'c': cmdGetAutoClock(data); break;
	case 'C': cmdSetAutoClock(data); break;
	case 'T': cmdSetWiFiTimer(data); break;
	case 'G': cmdGetDateTime(data); break;
	default:
		sendQuickResponse(cmd, 4); // ERR_INV_PARAM
		break;
	}
}

void GenericUNAPI::handleUnapiCommand(uint8_t cmd, std::span<const uint8_t> data)
{
	switch (cmd) {
	case 0:  cmdGetInfo(data); break;
	case 1:  cmdGetCapab(data); break;
	case 2:  cmdGetIPInfo(data); break;
	case 3:  cmdNetState(data); break;
	case 4: case 5: unimplementedCmd(cmd); break;
	case 6:  cmdDnsQ(data); break;
	case 7:  unimplementedCmd(cmd); break;
	case 8:  cmdUdpOpen(data); break;
	case 9:  cmdUdpClose(data); break;
	case 10: cmdUdpState(data); break;
	case 11: cmdUdpSend(data); break;
	case 12: cmdUdpRcv(data); break;
	case 13: cmdTcpOpen(data); break;
	case 14: cmdTcpClose(data); break;
	case 15: cmdTcpAbort(data); break;
	case 16: cmdTcpState(data); break;
	case 17: cmdTcpSend(data); break;
	case 18: cmdTcpRcv(data); break;
	case 19: unimplementedCmd(cmd); break;
	case 20: case 21: case 22: case 23: case 24:
		unimplementedCmd(cmd); break;
	case 25: cmdCfgAutoIP(data); break;
	case 26: cmdCfgIP(data); break;
	case 27: case 28: case 29:
		unimplementedCmd(cmd); break;
	case 206: cmdDnsQNew(data); break;
	default:
		unimplementedCmd(cmd);
		break;
	}
}

// ====================================================================
//  Helper: parse IP address string → 4 bytes
// ====================================================================

bool GenericUNAPI::parseIP(const std::string& str, uint8_t* ipOut) const
{
	unsigned a, b, c, d;
	if (sscanf(str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
	if (a > 255 || b > 255 || c > 255 || d > 255) return false;
	ipOut[0] = static_cast<uint8_t>(a);
	ipOut[1] = static_cast<uint8_t>(b);
	ipOut[2] = static_cast<uint8_t>(c);
	ipOut[3] = static_cast<uint8_t>(d);
	return true;
}

// ====================================================================
//  Custom command implementations
// ====================================================================

void GenericUNAPI::cmdReset(std::span<const uint8_t> /*data*/)
{
	// Close all connections
	{
		std::scoped_lock lock(connectionsMutex);
		for (int i = 0; i < MAX_CONNECTIONS; ++i) {
			freeConnection(i);
		}
	}
	lastResponse.clear();
	sendQuickResponse('R', '0');
	// Emulate the device reboot: the ESP thread will wipe pending input
	// and run the boot sequence (greeting + "Ready" loop).
	bootStartPending = true;
}

void GenericUNAPI::cmdWarmReset(std::span<const uint8_t> /*data*/)
{
	{
		std::scoped_lock lock(connectionsMutex);
		for (int i = 0; i < MAX_CONNECTIONS; ++i) {
			freeConnection(i);
		}
	}
	lastResponse.clear();
	// Raw text response, exactly like the firmware's Serial.println("Ready")
	static constexpr uint8_t msg[] = {'R', 'e', 'a', 'd', 'y', '\r', '\n'};
	sendRawResponse(msg);
}

void GenericUNAPI::cmdQuery(std::span<const uint8_t> /*data*/)
{
	// Raw text response (no framing — the MSX expects "OK")
	static constexpr uint8_t ok[] = {'O', 'K'};
	sendRawResponse(ok);
}

void GenericUNAPI::cmdGetVersion(std::span<const uint8_t> /*data*/)
{
	// Raw bytes, no framing: 'V' + major + minor as HEX values (not
	// ASCII) — the firmware writes chVer[0]-'0' and chVer[2]-'0'
	static constexpr uint8_t resp[] = {'V', 0x01, 0x00};
	sendRawResponse(resp);
}

void GenericUNAPI::cmdRetry(std::span<const uint8_t> /*data*/)
{
	if (lastResponse.empty()) {
		sendQuickResponse('r', 4); // no previous response
		return;
	}
	{
		std::scoped_lock lock(espToMsxMutex);
		for (auto b : lastResponse) {
			espToMsxFifo.push_back(b);
		}
	}
	espToMsxCond.notify_one();
}

void GenericUNAPI::cmdScanAP(std::span<const uint8_t> /*data*/)
{
	sendQuickResponse('S', 0);
}

void GenericUNAPI::cmdScanResults(std::span<const uint8_t> /*data*/)
{
	enum { SCAN_NO_NETWORKS = 2 };
	sendQuickResponse('s', SCAN_NO_NETWORKS);
}

void GenericUNAPI::cmdConnectAP(std::span<const uint8_t> /*data*/)
{
	enum { ERR_OK = 0 };
	sendQuickResponse('A', ERR_OK);
}

void GenericUNAPI::cmdGetAPStatus(std::span<const uint8_t> /*data*/)
{
	static constexpr uint8_t statusConnected = 5; // Connected and got IP
	static constexpr const char* apName = "GenericUNAPI";
	uint8_t apNameLen = static_cast<uint8_t>(strlen(apName));
	uint16_t respSize = 1 + apNameLen + 1; // status + name + null

	std::vector<uint8_t> resp;
	resp.reserve(respSize);
	resp.push_back(statusConnected);
	resp.insert(resp.end(), apName, apName + apNameLen);
	resp.push_back(0); // null terminator

	sendResponse('g', 0, resp);
}

void GenericUNAPI::cmdFirmwareUpdate(uint8_t cmd, std::span<const uint8_t> /*data*/)
{
	// All firmware / certificate update commands fail in the emulator.
	// Error codes mirror the firmware's failure paths:
	//  - 'B' (FILE_BOARD): FIRMWARETYPE mismatch
	//  - 'U'/'u' (UPDATE_FW / UPDATE_CERTS): OTA failure
	//  - 'Z'/'Y' (START_RS232_*): wrong data length (12 bytes required)
	//  - 'z' (BLOCK_RS232_UPDATE): no update in progress
	//  - 'E' (END_RS232_UPDATE): no update in progress
	//  - 'I' (INITCERTS): certificate store unavailable
	uint8_t error = 4; // UNAPI_ERR_INV_PARAM
	if (cmd == 'I') error = 3; // UNAPI_ERR_NO_DATA
	sendQuickResponse(cmd, error);
}

void GenericUNAPI::cmdSetBaud(std::span<const uint8_t> data)
{
	// data: single baud-rate index (0..BR859372, see ESP32BOARDS.h)
	if (data.size() != 1 || data[0] > 7) {
		sendQuickResponse('d', 4); // UNAPI_ERR_INV_PARAM
		return;
	}
	sendQuickResponse('d', 0);
}

void GenericUNAPI::cmdGetSettings(std::span<const uint8_t> /*data*/)
{
	// Framed response, like the firmware's
	// SendResponse(CUSTOM_F_QUERY_SETTINGS, OK, len, "OFF:30")
	static constexpr uint8_t settings[] = {
		'O', 'F', 'F', ':', '3', '0'
	};
	sendResponse('Q', 0, settings);
}

void GenericUNAPI::cmdGetAutoClock(std::span<const uint8_t> /*data*/)
{
	uint8_t resp[3] = {0, 0, 0}; // AutoClock=0 (off), GMT=0
	sendResponse('c', 0, {resp, 3});
}

void GenericUNAPI::cmdSetAutoClock(std::span<const uint8_t> data)
{
	// data: autoClock (0..3) + GMT offset byte
	if (data.size() != 2 || data[0] > 3) {
		sendQuickResponse('C', 4); // UNAPI_ERR_INV_PARAM
		return;
	}
	sendQuickResponse('C', 0);
}

void GenericUNAPI::cmdSetWiFiTimer(std::span<const uint8_t> data)
{
	// data: radio-off timer, 2 bytes (MSB first)
	if (data.size() != 2) {
		sendQuickResponse('T', 4); // UNAPI_ERR_INV_PARAM
		return;
	}
	sendQuickResponse('T', 0);
}

void GenericUNAPI::cmdGetDateTime(std::span<const uint8_t> /*data*/)
{
	// Return "no network" error since SNTP is not implemented
	enum { ERR_NO_NETWORK = 2 };
	sendQuickResponse('G', ERR_NO_NETWORK);
}

// ====================================================================
//  UNAPI command implementations
// ====================================================================

void GenericUNAPI::cmdGetInfo(std::span<const uint8_t> /*data*/)
{
	// UNAPI_GET_INFO — not normally sent over serial.
	// Return ERR_NOT_IMP as the MSX driver handles this locally.
	unimplementedCmd(0);
}

void GenericUNAPI::unimplementedCmd(uint8_t cmd)
{
	// All UNAPI responses use the framed format (CMD, ERR, SIZE, data)
	sendResponse(cmd, 1); // ERR_NOT_IMP
}

void GenericUNAPI::cmdGetCapab(std::span<const uint8_t> data)
{
	// Response blocks byte-match the ESP32 firmware: each block is
	// prefixed by its own data size byte.
	if (data.size() != 1 || data[0] == 0 || data[0] > 4) {
		sendResponse(1, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t block = data[0];
	switch (block) {
	case 1: {
		// Capability flags (2), feature flags (2), link level (1)
		uint8_t resp[5] = {0x2C, 0x44, 0x96, 0x1C, 0x04};
		sendResponse(1, 0, {resp, 5});
		break;
	}
	case 2: {
		// Max TCP (1), max UDP (1), free TCP (1), free UDP (1),
		// max raw TCP (1), max raw UDP (1)
		uint8_t resp[6] = {4, 4, 0, 0, 0, 0};
		{
			std::scoped_lock lock(connectionsMutex);
			int used = 0;
			for (auto& cp : connections) {
				if (cp && cp->type != 0) ++used;
			}
			resp[2] = 4 - used; // free TCP
			resp[3] = 4 - used; // free UDP
		}
		sendResponse(1, 0, {resp, 6});
		break;
	}
	case 3: {
		// Max incoming datagram size (2 LSB MSB), max outgoing (2 LSB MSB)
		uint8_t resp[4] = {
			1500 & 0xFF, (1500 >> 8) & 0xFF,
			2048 & 0xFF, (2048 >> 8) & 0xFF
		};
		sendResponse(1, 0, {resp, 4});
		break;
	}
	case 4: {
		// Secondary capability flags (2), feature flags (2), unused (1)
		// Bit 8 (use TLS in TCP active connections) is advertised only
		// when a host OpenSSL runtime is available; bit 9 (TLS in passive
		// connections) is never advertised.
		uint8_t resp[5] = {0xF7, 0x00, 0, 0, 0};
		if (OpenSSL::available()) {
			resp[1] |= 0x01; // bit 8: TLS in TCP active connections
		}
		sendResponse(1, 0, {resp, 5});
		break;
	}
	}
}

void GenericUNAPI::cmdGetIPInfo(std::span<const uint8_t> data)
{
	if (data.size() != 1 || data[0] == 0 || data[0] > 6) {
		sendResponse(2, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t idx = data[0];
	uint8_t ipBytes[4] = {};
	switch (idx) {
	case 1: // Local IP
	case 3: // Subnet mask
	case 4: // Default gateway
	case 5: // Primary DNS
	case 6: // Secondary DNS
	{
		// A manually configured setting overrides the host value;
		// otherwise fall back to the host's own network configuration
		// (like the ESP32 reporting its WiFi IP).
		const StringSetting* setting = nullptr;
		switch (idx) {
		case 1: setting = &localIpSetting; break;
		case 3: setting = &subnetSetting; break;
		case 4: setting = &gatewaySetting; break;
		case 5: setting = &dnsPrimarySetting; break;
		case 6: setting = &dnsSecondarySetting; break;
		}
		std::string ipStr = std::string(setting->getString());
		bool useSetting = parseIP(ipStr, ipBytes);
		if (useSetting && ipBytes[0] == 0 && ipBytes[1] == 0 &&
		    ipBytes[2] == 0 && ipBytes[3] == 0) {
			useSetting = false; // "0.0.0.0" means "use host value"
		}
		if (!useSetting) {
			SockNetInfo net;
			if (sock_get_net_info(net)) {
				uint32_t ip = 0;
				switch (idx) {
				case 1: ip = net.ip; break;
				case 3: ip = net.netmask; break;
				case 4: ip = net.gateway; break;
				case 5: ip = net.dns1; break;
				case 6: ip = net.dns2; break;
				}
				ipBytes[0] = (ip >> 24) & 0xFF;
				ipBytes[1] = (ip >> 16) & 0xFF;
				ipBytes[2] = (ip >> 8) & 0xFF;
				ipBytes[3] = ip & 0xFF;
			}
		}
		break;
	}
	case 2: // Peer IP — not applicable
		break;
	default:
		sendResponse(2, 4); // ERR_INV_PARAM
		return;
	}
	sendResponse(2, 0, {ipBytes, 4});
}

void GenericUNAPI::cmdNetState(std::span<const uint8_t> data)
{
	if (!data.empty()) {
		sendResponse(3, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t state = enabledSetting.getBoolean() ? 2 : 0; // 2=Open, 0=Closed
	sendResponse(3, 0, {&state, 1});
}

void GenericUNAPI::cmdDnsQ(std::span<const uint8_t> data)
{
	// data: zero-terminated host name string
	// Find null terminator
	if (data.empty()) {
		sendResponse(6, 4); // ERR_INV_PARAM
		return;
	}
	// Build null-terminated string
	std::string hostname(reinterpret_cast<const char*>(data.data()),
	                     data.size());
	// Remove trailing null if present
	if (!hostname.empty() && hostname.back() == '\0') {
		hostname.pop_back();
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo* res = nullptr;
	int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
	if (err != 0 || !res) {
		if (res) freeaddrinfo(res);
		sendResponse(6, 8); // ERR_DNS
		return;
	}

	auto* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
	uint32_t ip = ntohl(addr->sin_addr.s_addr);
	uint8_t ipBytes[4] = {
		static_cast<uint8_t>((ip >> 24) & 0xFF),
		static_cast<uint8_t>((ip >> 16) & 0xFF),
		static_cast<uint8_t>((ip >> 8) & 0xFF),
		static_cast<uint8_t>(ip & 0xFF)
	};
	freeaddrinfo(res);

	sendResponse(6, 0, {ipBytes, 4});
}

void GenericUNAPI::cmdDnsQNew(std::span<const uint8_t> data)
{
	// data[0] = flags (only bits 0-2 are defined), data[1..] =
	// zero-terminated host name string
	if (data.size() < 2 || (data[0] & 0xf8)) {
		sendResponse(206, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t flags = data[0];
	auto hostnameSpan = data.subspan(1);
	std::string hostname(reinterpret_cast<const char*>(hostnameSpan.data()),
	                     hostnameSpan.size());
	if (!hostname.empty() && hostname.back() == '\0') {
		hostname.pop_back();
	}

	// If the string is a valid IP address, use it directly
	uint8_t ipBytes[4];
	if (parseIP(hostname, ipBytes)) {
		sendResponse(206, 0, {ipBytes, 4});
		return;
	}
	if (flags & 2) {
		sendResponse(206, 6); // ERR_INV_IP
		return;
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo* res = nullptr;
	int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
	if (err != 0 || !res) {
		if (res) freeaddrinfo(res);
		sendResponse(206, 8); // ERR_DNS
		return;
	}

	auto* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
	uint32_t ipN = ntohl(addr->sin_addr.s_addr);
	uint8_t ipB[4] = {
		static_cast<uint8_t>((ipN >> 24) & 0xFF),
		static_cast<uint8_t>((ipN >> 16) & 0xFF),
		static_cast<uint8_t>((ipN >> 8) & 0xFF),
		static_cast<uint8_t>(ipN & 0xFF)
	};
	freeaddrinfo(res);

	sendResponse(206, 0, {ipB, 4});
}

void GenericUNAPI::cmdUdpOpen(std::span<const uint8_t> data)
{
	// Parameters: localPort (2, LSB MSB) + transient flag (1)
	if (data.size() != 3) {
		sendResponse(8, 4); // ERR_INV_PARAM
		return;
	}
	uint16_t localPort = data[0] | (static_cast<uint16_t>(data[1]) << 8);
	uint8_t lifetime = data[2]; // transient flag
	(void)lifetime;

	if (localPort == 0 || lifetime > 1 ||
	    (localPort >= 0xfff0 && localPort != 0xffff)) {
		// Port 0 is unsupported and FFF0h-FFFEh are reserved (spec 4.4.1)
		sendResponse(8, 4); // ERR_INV_PARAM
		return;
	}

	// FFFFh port: random port in 16384-32767, never a port number used by
	// another open UDP connection (spec 4.4.1)
	if (localPort == 0xFFFF) {
		std::scoped_lock lock(connectionsMutex);
		auto udpPortInUse = [&](uint16_t port) {
			for (auto& cp : connections) {
				if (cp && cp->type == 2 && cp->localPort == port) {
					return true;
				}
			}
			return false;
		};
		do {
			localPort = static_cast<uint16_t>(16384 + (std::rand() % 16384));
		} while (udpPortInUse(localPort));
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == OPENMSX_INVALID_SOCKET) {
		sendResponse(8, 2); // ERR_NO_NETWORK
		return;
	}

	// Bind to local port
	{
		struct sockaddr_in bindAddr;
		memset(&bindAddr, 0, sizeof(bindAddr));
		bindAddr.sin_family = AF_INET;
		bindAddr.sin_port = htons(localPort);
		bindAddr.sin_addr.s_addr = INADDR_ANY;
		if (bind(s, reinterpret_cast<struct sockaddr*>(&bindAddr),
		         sizeof(bindAddr)) < 0) {
			sock_close(s);
			sendResponse(8, 10); // ERR_CONN_EXISTS
			return;
		}
	}

	// Make socket non-blocking
	setNonBlocking(s);

	std::scoped_lock lock(connectionsMutex);
	auto* conn = allocateConnection();
	if (!conn) {
		sock_close(s);
		sendResponse(8, 9); // ERR_NO_FREE_CONN
		return;
	}
	conn->type = 2; // UDP
	conn->sock = s;
	conn->localPort = localPort;
	conn->state = 2; // OPEN

	int connIdx = 0;
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (connections[i].get() == conn) {
			connIdx = i;
			break;
		}
	}
	// Connection numbers are 1-based, as in the real firmware
	uint8_t connNum = static_cast<uint8_t>(connIdx + 1);
	sendResponse(8, 0, {&connNum, 1});
}

void GenericUNAPI::cmdUdpClose(std::span<const uint8_t> data)
{
	if (data.size() != 1) {
		sendResponse(9, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num > MAX_CONNECTIONS) {
		sendResponse(9, 11); // ERR_NO_CONN
		return;
	}
	std::scoped_lock lock(connectionsMutex);
	if (num == 0) {
		// Close all open (transient) UDP connections
		for (int i = 0; i < MAX_CONNECTIONS; ++i) {
			if (connections[i] && connections[i]->type == 2) {
				freeConnection(i);
			}
		}
		sendResponse(9, 0, {});
		return;
	}
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 2) {
		sendResponse(9, 11); // ERR_NO_CONN
		return;
	}
	freeConnection(num - 1);
	sendResponse(9, 0, {});
}

void GenericUNAPI::cmdUdpState(std::span<const uint8_t> data)
{
	if (data.size() != 1) {
		sendResponse(10, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num == 0 || num > MAX_CONNECTIONS) {
		sendResponse(10, 11); // ERR_NO_CONN
		return;
	}
	std::scoped_lock lock(connectionsMutex);
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 2) {
		sendResponse(10, 11); // ERR_NO_CONN
		return;
	}
	// Return: local port (2), pending datagrams (1), oldest size (2)
	uint16_t port = conn->localPort;
	// Count pending datagrams is tricky with raw sockets — just return 0
	uint8_t pendingDgrams = 0;
	uint16_t oldestSize = 0;
	uint8_t resp[5] = {
		static_cast<uint8_t>(port & 0xFF),
		static_cast<uint8_t>((port >> 8) & 0xFF),
		pendingDgrams,
		static_cast<uint8_t>(oldestSize & 0xFF),
		static_cast<uint8_t>((oldestSize >> 8) & 0xFF)
	};
	sendResponse(10, 0, {resp, 5});
}

void GenericUNAPI::cmdUdpSend(std::span<const uint8_t> data)
{
	// conn num (1) + remote IP (4) + remote port (2) + payload
	if (data.size() < 8) {
		sendResponse(11, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num == 0 || num > MAX_CONNECTIONS) {
		sendResponse(11, 11); // ERR_NO_CONN
		return;
	}
	uint32_t destIP = (static_cast<uint32_t>(data[1]) << 24) |
	                  (static_cast<uint32_t>(data[2]) << 16) |
	                  (static_cast<uint32_t>(data[3]) << 8) |
	                  static_cast<uint32_t>(data[4]);
	uint16_t destPort = data[5] | (static_cast<uint16_t>(data[6]) << 8);

	std::scoped_lock lock(connectionsMutex);
	auto* conn = getConnection(num - 1);
	if (!conn) {
		sendResponse(11, 11); // ERR_NO_CONN
		return;
	}
	if (conn->type != 2) {
		sendResponse(11, 2); // ERR_NO_NETWORK (firmware quirk)
		return;
	}

	struct sockaddr_in to;
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(destPort);
	to.sin_addr.s_addr = htonl(destIP);

	auto payload = data.subspan(7);
	auto n = sock_sendto(conn->sock,
	                     reinterpret_cast<const char*>(payload.data()),
	                     payload.size(),
	                     reinterpret_cast<struct sockaddr*>(&to),
	                     sizeof(to));
	if (n < 0 || static_cast<size_t>(n) != payload.size()) {
		sendResponse(11, 2); // ERR_NO_NETWORK
		return;
	}
	sendResponse(11, 0, {});
}

void GenericUNAPI::cmdUdpRcv(std::span<const uint8_t> data)
{
	if (data.size() != 3) {
		sendResponse(12, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num == 0 || num > MAX_CONNECTIONS) {
		sendResponse(12, 11); // ERR_NO_CONN
		return;
	}
	uint16_t maxSize = data[1] | (static_cast<uint16_t>(data[2]) << 8);
	if (maxSize > 2048) maxSize = 2048;

	std::scoped_lock lock(connectionsMutex);
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 2) {
		sendResponse(12, 11); // ERR_NO_CONN
		return;
	}

	// Non-blocking receive
	std::vector<char> buf(maxSize > 0 ? maxSize : 2048);
	struct sockaddr_in from;
	socklen_t fromLen = sizeof(from);
	auto n = sock_recvfrom(conn->sock, buf.data(), buf.size(),
	                       reinterpret_cast<struct sockaddr*>(&from),
	                       &fromLen);
	if (n <= 0) {
		sendResponse(12, 3); // ERR_NO_DATA
		return;
	}

	uint32_t srcIP = ntohl(from.sin_addr.s_addr);
	uint16_t srcPort = ntohs(from.sin_port);

	// Response: remote IP (4) + remote port (2 LSB MSB) + data
	std::vector<uint8_t> resp;
	resp.reserve(6 + n);
	resp.push_back((srcIP >> 24) & 0xFF);
	resp.push_back((srcIP >> 16) & 0xFF);
	resp.push_back((srcIP >> 8) & 0xFF);
	resp.push_back(srcIP & 0xFF);
	resp.push_back(srcPort & 0xFF);
	resp.push_back((srcPort >> 8) & 0xFF);
	resp.insert(resp.end(), buf.begin(), buf.begin() + n);

	sendResponse(12, 0, resp);
}

void GenericUNAPI::cmdTcpOpen(std::span<const uint8_t> data)
{
	// Parameters (11+ bytes): remoteIP(4) + remotePort(2 LSB MSB)
	//  + localPort(2 LSB MSB) + user timeout (2) + flags (1)
	// flags: bit0 passive, bit1 resident, bit2 TLS, bit3 verify certs,
	// bits 4-7 unused, must be zero (spec 4.5.1)
	if (data.size() < 11) {
		sendResponse(13, 4); // ERR_INV_PARAM
		return;
	}
	uint32_t remoteIP = (static_cast<uint32_t>(data[0]) << 24) |
	                    (static_cast<uint32_t>(data[1]) << 16) |
	                    (static_cast<uint32_t>(data[2]) << 8) |
	                    static_cast<uint32_t>(data[3]);
	uint16_t remotePort = data[4] | (static_cast<uint16_t>(data[5]) << 8);
	uint16_t localPort = data[6] | (static_cast<uint16_t>(data[7]) << 8);
	uint16_t userTimeout = data[8] | (static_cast<uint16_t>(data[9]) << 8);
	uint8_t flags = data[10];

	// User timeout: 0 (default), 1-1080 (seconds) or FFFFh (infinite) are
	// the only valid values (spec 4.5.1)
	if (userTimeout != 0 && userTimeout != 0xFFFF &&
	    (userTimeout < 1 || userTimeout > 1080)) {
		sendResponse(13, 4); // ERR_INV_PARAM
		return;
	}
	if (flags & 0xf0) {
		sendResponse(13, 4); // ERR_INV_PARAM (unused flag set)
		return;
	}
	if (localPort == 0) {
		sendResponse(13, 4); // ERR_INV_PARAM
		return;
	}

	// The optional server host name (for TLS certificate validation)
	// is passed inline, right after the fixed parameters block, like the
	// ESP32 firmware reads it (spec 4.5.1 defines it as an MSX address,
	// but the firmware takes it from the command data stream).
	std::string hostname;
	if (data.size() > 11) {
		size_t len = std::min<size_t>(data.size() - 11, 255);
		hostname.assign(reinterpret_cast<const char*>(data.data() + 11), len);
		auto nul = hostname.find('\0');
		if (nul != std::string::npos) hostname.resize(nul);
	}

	bool passive = (flags & 1) != 0;
	bool useTls = (flags & 4) != 0;
	bool verifyCert = (flags & 8) != 0;
	if (passive) {
		// Passive with a specified remote socket is not advertised
		// (capability bit 4) and TLS in passive connections is not
		// advertised (capability bit 9): both are ERR_NOT_IMP per spec
		if (remoteIP != 0 || useTls) {
			sendResponse(13, 1); // ERR_NOT_IMP
			return;
		}
	} else {
		// Active mode requires a remote address. TLS in active connections
		// requires a host OpenSSL runtime (capability bit 8 is not
		// advertised otherwise) -> ERR_NOT_IMP.
		if (remoteIP == 0) {
			sendResponse(13, 4); // ERR_INV_PARAM
			return;
		}
		if (useTls && !OpenSSL::available()) {
			sendResponse(13, 1); // ERR_NOT_IMP
			return;
		}
	}
	// Flag bit 3 (verify certificate) is only meaningful with TLS on an
	// active connection; otherwise it is ignored (spec 4.5.1)

	std::unique_lock lock(connectionsMutex);

	// FFFFh local port: random port in 16384-32767, never a port number
	// used by another open TCP connection (spec 4.5.1)
	if (localPort == 0xFFFF) {
		auto tcpPortInUse = [&](uint16_t port) {
			for (auto& cp : connections) {
				if (cp && cp->type == 1 && cp->localPort == port) {
					return true;
				}
			}
			return false;
		};
		do {
			localPort = static_cast<uint16_t>(16384 + (std::rand() % 16384));
		} while (tcpPortInUse(localPort));
	}

	if (passive) {
		// Find an existing shared listener for this port, or create a new
		// one (passive connections with unspecified remote socket may
		// share a local port, spec 4.5.1)
		ListenSocket* entry = nullptr;
		int entryIdx = -1;
		for (size_t i = 0; i < listenSockets.size(); ++i) {
			if (listenSockets[i]->port == localPort) {
				entry = listenSockets[i].get();
				entryIdx = static_cast<int>(i);
				break;
			}
		}
		if (!entry) {
			SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (ls == OPENMSX_INVALID_SOCKET) {
				sendResponse(13, 2); // ERR_NO_NETWORK
				return;
			}
			int one = 1;
			setsockopt(ls, SOL_SOCKET, SO_REUSEADDR,
			           std::bit_cast<char*>(&one), sizeof(one));
			struct sockaddr_in bindAddr;
			memset(&bindAddr, 0, sizeof(bindAddr));
			bindAddr.sin_family = AF_INET;
			bindAddr.sin_port = htons(localPort);
			bindAddr.sin_addr.s_addr = INADDR_ANY;
			if (bind(ls, reinterpret_cast<struct sockaddr*>(&bindAddr),
			         sizeof(bindAddr)) < 0) {
				sock_close(ls);
				sendResponse(13, 10); // ERR_CONN_EXISTS
				return;
			}
			if (listen(ls, MAX_CONNECTIONS) < 0) {
				sock_close(ls);
				sendResponse(13, 10); // ERR_CONN_EXISTS
				return;
			}
			setNonBlocking(ls);
			auto up = std::make_unique<ListenSocket>();
			up->sock = ls;
			up->port = localPort;
			up->refs = 1;
			entryIdx = static_cast<int>(listenSockets.size());
			listenSockets.push_back(std::move(up));
			entry = listenSockets.back().get();
		} else {
			++entry->refs;
		}

		Connection* conn = allocateConnection();
		if (!conn) {
			releaseListenEntry(entryIdx);
			sendResponse(13, 9); // ERR_NO_FREE_CONN
			return;
		}
		int connIdx = 0;
		for (int i = 0; i < MAX_CONNECTIONS; ++i) {
			if (connections[i].get() == conn) {
				connIdx = i;
				break;
			}
		}
		conn->type = 1; // TCP
		conn->sock = entry->sock;
		conn->listenSock = entry->sock;
		conn->listenIdx = entryIdx;
		conn->clientSock = OPENMSX_INVALID_SOCKET;
		conn->clientEof = false;
		conn->remoteIP = 0;
		conn->remotePort = 0;
		conn->localPort = localPort;
		conn->flags = flags;
		conn->state = 2; // open (LISTEN until a client is accepted)

		// Start reader thread (accepts clients and reads inbound data)
		conn->readerActive = true;
		conn->readerThread = std::make_unique<std::thread>(
		        [this, connIdx] { tcpReaderThreadFunc(connIdx); });

		uint8_t connNum = static_cast<uint8_t>(connIdx + 1);
		sendResponse(13, 0, {&connNum, 1});
		return;
	}

	// Active connection: check that no TCP connection is already open with
	// the same combination of local port, remote IP and remote port
	// (spec: ERR_CONN_EXISTS), and that no passive listener owns the port
	for (auto& cp : connections) {
		if (cp && cp->type == 1 && cp->listenIdx < 0 &&
		    cp->localPort == localPort && cp->remoteIP == remoteIP &&
		    cp->remotePort == remotePort) {
			sendResponse(13, 10); // ERR_CONN_EXISTS
			return;
		}
	}
	for (auto& e : listenSockets) {
		if (e->port == localPort) {
			sendResponse(13, 10); // ERR_CONN_EXISTS
			return;
		}
	}
	if (getFreeConnectionSlot() < 0) {
		sendResponse(13, 9); // ERR_NO_FREE_CONN
		return;
	}
	lock.unlock();

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == OPENMSX_INVALID_SOCKET) {
		sendResponse(13, 2); // ERR_NO_NETWORK
		return;
	}

	// TCP_NODELAY
	int one = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
	           std::bit_cast<char*>(&one), sizeof(one));
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	           std::bit_cast<char*>(&one), sizeof(one));

	// Bind local port (may fail if e.g. an OS-level socket owns it)
	{
		struct sockaddr_in bindAddr;
		memset(&bindAddr, 0, sizeof(bindAddr));
		bindAddr.sin_family = AF_INET;
		bindAddr.sin_port = htons(localPort);
		bindAddr.sin_addr.s_addr = INADDR_ANY;
		if (bind(s, reinterpret_cast<struct sockaddr*>(&bindAddr),
		         sizeof(bindAddr)) < 0) {
			sock_close(s);
			sendResponse(13, 10); // ERR_CONN_EXISTS
			return;
		}
	}

	// Connect
	struct sockaddr_in dest;
	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(remotePort);
	dest.sin_addr.s_addr = htonl(remoteIP);

	if (connect(s, reinterpret_cast<struct sockaddr*>(&dest),
	            sizeof(dest)) < 0) {
		sock_close(s);
		// Return error with close reason byte (unknown reason)
		uint8_t reasonByte = 0;
		sendResponse(13, 11, {&reasonByte, 1}); // ERR_NO_CONN
		return;
	}

	// Make socket non-blocking (reader thread polls for data)
	setNonBlocking(s);

	// TLS: create the OpenSSL session on the connected socket; the
	// handshake itself runs in the reader thread (the connection reports
	// SYN-SENT until it is finished, spec 4.5.5). The host name is used
	// for SNI and, when verifying, for the certificate host name check.
	void* ssl = nullptr;
	if (useTls) {
		ssl = OpenSSL::createClientSession(
			verifyCert, hostname.empty() ? nullptr : hostname.c_str(),
			static_cast<int>(s));
		if (!ssl) {
			sock_close(s);
			uint8_t reasonByte = 19; // TLS: other error
			sendResponse(13, 11, {&reasonByte, 1}); // ERR_NO_CONN
			return;
		}
	}

	lock.lock();
	auto* conn = allocateConnection();
	// A free slot was checked before connecting; since all commands run
	// on the same ESP thread, the slot is still free.
	if (!conn) {
		if (ssl) OpenSSL::close(ssl);
		sock_close(s);
		sendResponse(13, 9); // ERR_NO_FREE_CONN
		return;
	}
	int connIdx = 0;
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (connections[i].get() == conn) {
			connIdx = i;
			break;
		}
	}
	conn->type = 1; // TCP
	conn->sock = s;
	conn->remoteIP = remoteIP;
	conn->remotePort = remotePort;
	conn->localPort = localPort;
	conn->flags = flags;
	conn->ssl = ssl;
	conn->tlsVerify = verifyCert;
	conn->handshakePhase = ssl ? 1 : 0; // 1 = TLS handshake in progress
	// Without TLS the TCP connect() already finished, so the connection is
	// ESTABLISHED right away. With TLS it reports SYN-SENT (state=1) until
	// the handshake and certificate validation have completed (spec 4.5.5:
	// TCP_STATE must not report ESTABLISHED before the handshake is done).
	conn->state = ssl ? 1 : 2;

	// Start reader thread
	conn->readerActive = true;
	conn->readerThread = std::make_unique<std::thread>(
	        [this, connIdx] { tcpReaderThreadFunc(connIdx); });

	uint8_t connNum = static_cast<uint8_t>(connIdx + 1);
	sendResponse(13, 0, {&connNum, 1});
}

void GenericUNAPI::cmdTcpClose(std::span<const uint8_t> data)
{
	if (data.size() != 1) {
		sendResponse(14, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num > MAX_CONNECTIONS) {
		sendResponse(14, 11); // ERR_NO_CONN
		return;
	}
	std::scoped_lock lock(connectionsMutex);
	if (num == 0) {
		// Close all open transient TCP connections; resident connections
		// (opened with flags bit 1 set) are kept
		for (int i = 0; i < MAX_CONNECTIONS; ++i) {
			if (connections[i] && connections[i]->type == 1 &&
			    !(connections[i]->flags & 2)) {
				freeConnection(i);
			}
		}
		sendResponse(14, 0, {});
		return;
	}
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 1) {
		sendResponse(14, 11); // ERR_NO_CONN
		return;
	}
	conn->state = 3; // closing
	freeConnection(num - 1);
	sendResponse(14, 0, {});
}

void GenericUNAPI::cmdTcpAbort(std::span<const uint8_t> data)
{
	if (data.size() != 1) {
		sendResponse(15, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num > MAX_CONNECTIONS) {
		sendResponse(15, 11); // ERR_NO_CONN
		return;
	}
	std::scoped_lock lock(connectionsMutex);
	if (num == 0) {
		// Abort all open transient TCP connections; resident connections
		// (opened with flags bit 1 set) are kept
		for (int i = 0; i < MAX_CONNECTIONS; ++i) {
			if (connections[i] && connections[i]->type == 1 &&
			    !(connections[i]->flags & 2)) {
				freeConnection(i);
			}
		}
		sendResponse(15, 0, {});
		return;
	}
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 1) {
		sendResponse(15, 11); // ERR_NO_CONN
		return;
	}
	conn->state = 3;
	freeConnection(num - 1);
	sendResponse(15, 0, {});
}

void GenericUNAPI::cmdTcpState(std::span<const uint8_t> data)
{
	if (data.size() != 1) {
		sendResponse(16, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num == 0 || num > MAX_CONNECTIONS) {
		sendResponse(16, 11); // ERR_NO_CONN
		return;
	}
	std::scoped_lock lock(connectionsMutex);
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 1) {
		sendResponse(16, 11); // ERR_NO_CONN
		return;
	}

	// Connection failed (e.g. TLS handshake or certificate error): report
	// ERR_NO_CONN with the close reason stored when it failed (spec 4.5.4)
	if (conn->state == 4) {
		uint8_t reason = closeReason[num - 1].load(std::memory_order_relaxed);
		sendResponse(16, 11, {&reason, 1}); // ERR_NO_CONN
		return;
	}

	// Passive connection with no client attached yet: LISTEN state, only
	// the local port is meaningful (like the firmware's TcpState)
	if (conn->listenSock != OPENMSX_INVALID_SOCKET &&
	    conn->clientSock == OPENMSX_INVALID_SOCKET) {
		uint8_t resp[16] = {};
		resp[1] = 1; // LISTEN
		resp[14] = conn->localPort & 0xFF;
		resp[15] = (conn->localPort >> 8) & 0xFF;
		sendResponse(16, 0, {resp, 16});
		return;
	}

	uint16_t avail = 0;
	{
		std::scoped_lock rlock(conn->recvMutex);
		avail = static_cast<uint16_t>(conn->recvBuffer.size());
	}

	// Info block, same layout as the firmware's TcpState():
	// TLS flag (1) + lwIP state (1) + available (2 LSB MSB)
	// + urgent (2) + send space (2 LSB MSB, capped at 2048)
	// + remote IP (4) + remote port (2 LSB MSB) + local port (2 LSB MSB)
	uint8_t resp[16] = {};
	resp[0] = conn->ssl ? 1 : 0;  // TLS flag (spec 4.5.5, bit 0)
	// Report the live connection state as the firmware does (raw lwIP
	// TCP state): the reader thread sets state=3 when the remote side
	// closes the connection, which maps to CLOSE_WAIT (7). Per the spec's
	// robustness recommendation, keep reporting ESTABLISHED while there
	// is still unconsumed incoming data.
	uint8_t tcpState = 4;              // ESTABLISHED
	if (conn->state == 1) {
		tcpState = 2;              // opening -> SYN_SENT
	} else if (conn->state == 3) {
		tcpState = (avail > 0) ? 4 : 7; // CLOSE_WAIT (7), ESTABLISHED (4) if data pending
	}
	resp[1] = tcpState;
	resp[2] = avail & 0xFF;
	resp[3] = (avail >> 8) & 0xFF;
	resp[4] = 0;                       // no urgent data
	resp[5] = 0;
	uint16_t sendSpace = 2048;         // arbitrary, capped like the firmware
	resp[6] = sendSpace & 0xFF;
	resp[7] = (sendSpace >> 8) & 0xFF;
	resp[8]  = (conn->remoteIP >> 24) & 0xFF;
	resp[9]  = (conn->remoteIP >> 16) & 0xFF;
	resp[10] = (conn->remoteIP >> 8) & 0xFF;
	resp[11] = conn->remoteIP & 0xFF;
	resp[12] = conn->remotePort & 0xFF;
	resp[13] = (conn->remotePort >> 8) & 0xFF;
	resp[14] = conn->localPort & 0xFF;
	resp[15] = (conn->localPort >> 8) & 0xFF;

	sendResponse(16, 0, {resp, 16});
}

void GenericUNAPI::cmdTcpSend(std::span<const uint8_t> data)
{
	// conn num (1) + flags (1) + payload. Per spec: bit 0 = PUSH, bit 1 =
	// urgent (ignored: urgent data not supported); bits 2-7 are unused.
	if (data.size() < 3) {
		sendResponse(17, 11); // ERR_NO_CONN (cannot parse conn number)
		return;
	}
	if (data[1] & 0xfc) {
		sendResponse(17, 4); // ERR_INV_PARAM (unused flag set)
		return;
	}
	int num = data[0];
	if (num == 0 || num > MAX_CONNECTIONS) {
		sendResponse(17, 11); // ERR_NO_CONN
		return;
	}
	uint8_t sendFlags = data[1];
	(void)sendFlags;

	std::scoped_lock lock(connectionsMutex);
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 1) {
		sendResponse(17, 11); // ERR_NO_CONN
		return;
	}
	if (conn->listenSock != OPENMSX_INVALID_SOCKET &&
	    conn->clientSock == OPENMSX_INVALID_SOCKET) {
		// Connection is in LISTEN state (spec: ERR_CONN_STATE)
		sendResponse(17, 12); // ERR_CONN_STATE
		return;
	}
	// Passive connections send through the accepted client socket; active
	// ones through the connected socket.
	SOCKET sendSock = (conn->listenSock != OPENMSX_INVALID_SOCKET)
	                ? conn->clientSock : conn->sock;

	auto payload = data.subspan(2);
	ptrdiff_t n;
	if (conn->ssl) {
		// TLS: sending is only possible once the handshake has finished;
		// before that the connection is still in SYN-SENT (spec 4.5.2:
		// data can only be sent on ESTABLISHED/CLOSE_WAIT connections)
		if (conn->handshakePhase.load(std::memory_order_relaxed) != 2) {
			sendResponse(17, 12); // ERR_CONN_STATE
			return;
		}
		int want = 0;
		n = OpenSSL::write(conn->ssl,
		                   reinterpret_cast<const char*>(payload.data()),
		                   payload.size(), want);
		if (n == 0) {
			// Peer sent close_notify: the connection is closed
			conn->state = 3; // remote closed -> CLOSE_WAIT
			sendResponse(17, 12); // ERR_CONN_STATE
			return;
		}
	} else {
		n = sock_send(sendSock,
		              reinterpret_cast<const char*>(payload.data()),
		              payload.size());
	}
	if (n < 0) {
		sendResponse(17, 12); // ERR_CONN_STATE
		return;
	}
	if (static_cast<size_t>(n) != payload.size()) {
		sendResponse(17, 13); // ERR_BUFFER
		return;
	}
	sendResponse(17, 0, {});
}

void GenericUNAPI::cmdTcpRcv(std::span<const uint8_t> data)
{
	if (data.size() != 3) {
		sendResponse(18, 4); // ERR_INV_PARAM
		return;
	}
	int num = data[0];
	if (num == 0 || num > MAX_CONNECTIONS) {
		sendResponse(18, 11); // ERR_NO_CONN
		return;
	}
	uint16_t maxSize = data[1] | (static_cast<uint16_t>(data[2]) << 8);
	if (maxSize > 2048) maxSize = 2048;

	std::scoped_lock lock(connectionsMutex);
	auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 1) {
		sendResponse(18, 11); // ERR_NO_CONN
		return;
	}
	if (conn->listenSock != OPENMSX_INVALID_SOCKET &&
	    conn->clientSock == OPENMSX_INVALID_SOCKET) {
		// Connection is in LISTEN state: no data can be retrieved, but
		// per spec 4.5.6 RCV can be called regardless of the state and
		// simply returns zero bytes with ERR_OK
		sendResponse(18, 0, {});
		return;
	}

	std::vector<uint8_t> buf;
	{
		std::scoped_lock rlock(conn->recvMutex);
		size_t avail = conn->recvBuffer.size();
		size_t toRead = std::min<size_t>(avail, maxSize);
		buf.reserve(2 + toRead);
		for (size_t i = 0; i < toRead; ++i) {
			buf.push_back(conn->recvBuffer.front());
			conn->recvBuffer.pop_front();
		}
	}

	if (buf.empty()) {
		// Firmware: OK response with size 0 (no urgent pointer bytes)
		sendResponse(18, 0, {});
		return;
	}

	// Response: urgent(2 LSB MSB) + data
	std::vector<uint8_t> resp;
	resp.reserve(2 + buf.size());
	resp.push_back(0); // urgent LSB
	resp.push_back(0); // urgent MSB (no urgent data)
	resp.insert(resp.end(), buf.begin(), buf.end());

	sendResponse(18, 0, resp);
}

void GenericUNAPI::cmdCfgAutoIP(std::span<const uint8_t> data)
{
	// action (1) + config (1, bits: IP automatic / DNS automatic)
	if (data.size() != 2 || data[0] > 1 || data[1] > 3) {
		sendResponse(25, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t action = data[0];
	uint8_t config = data[1];
	uint8_t result = config; // just return the requested value
	if (action == 0) { // GET
		result = 3; // DHCP for IP + DNS
	}
	sendResponse(25, 0, {&result, 1});
}

void GenericUNAPI::cmdCfgIP(std::span<const uint8_t> data)
{
	// field (1, 1..6 except 2) + IP (4)
	if (data.size() != 5 || data[0] == 0 || data[0] == 2 || data[0] > 6) {
		sendResponse(26, 4); // ERR_INV_PARAM
		return;
	}
	// Just store in settings (optional) and return OK
	sendResponse(26, 0, {});
}

// ====================================================================
//  Serialization
// ====================================================================

template<typename Archive>
void GenericUNAPI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// Connection state is not serialized (connections are ephemeral).
	// On load the device will be reset.
	if constexpr (Archive::IS_LOADER) {
		reset(EmuTime::zero());
	}
}
INSTANTIATE_SERIALIZE_METHODS(GenericUNAPI);
REGISTER_MSXDEVICE(GenericUNAPI, "GenericUNAPI");

} // namespace openmsx

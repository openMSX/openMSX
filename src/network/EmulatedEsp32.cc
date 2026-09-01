#include "EmulatedEsp32.hh"

#include "endian.hh"
#include "MSXCliComm.hh"
#include "narrow.hh"
#include "OpenSSL.hh"
#include "Poller.hh"
#include "stl.hh"
#include "StringOp.hh"
#include "zstring_view.hh"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <string_view>

#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <iphlpapi.h>
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
	default:
		break;
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

// ---- helper: host network configuration ----
// Detects the host's primary IPv4 network configuration. Used as a
// fallback for GET_IPINFO when the corresponding setting is 0.0.0.0
// ("use host value"), like the ESP32 reporting its WiFi IP.
struct HostNetInfo
{
	uint32_t ip = 0;
	uint32_t netmask = 0;
	uint32_t gateway = 0;
	uint32_t dns1 = 0;
	uint32_t dns2 = 0;
};

// Parse a dotted-quad IPv4 string into a value in network byte order
// (0 on failure)
static bool parseIPv4String(std::string_view str, uint32_t& ipOut)
{
	std::array<unsigned, 4> octets;
	int n = 0;
	std::string_view rest = str;
	while (n < 4) {
		auto dot = rest.find('.');
		auto v = StringOp::stringToBase<10, unsigned>(rest.substr(0, dot));
		if (!v || *v > 255) return false;
		octets[n++] = *v;
		if (dot == std::string_view::npos) break;
		rest.remove_prefix(dot + 1);
	}
	if (n != 4) return false;
	ipOut = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
	return true;
}

#ifndef _WIN32
// Read the first two DNS servers from /etc/resolv.conf
static void readResolvConfDns(uint32_t& dns1, uint32_t& dns2)
{
	std::ifstream f("/etc/resolv.conf");
	std::string line;
	int n = 0;
	while (n < 2 && std::getline(f, line)) {
		auto pos = line.find_first_not_of(" \t");
		if (pos == std::string::npos) continue;
		std::string_view entry(line);
		entry.remove_prefix(pos);
		if (!entry.starts_with("nameserver ")) continue;
		entry.remove_prefix(11);
		entry = entry.substr(0, entry.find('#'));
		entry = entry.substr(0, entry.find_last_not_of(" \t") + 1);
		uint32_t ip = 0;
		if (parseIPv4String(entry, ip)) {
			if (n == 0) {
				dns1 = ip;
			} else {
				dns2 = ip;
			}
			++n;
		}
	}
}
#endif

#ifdef _WIN32
// Adapter name markers of virtual network adapters (virtual switches,
// VM/VPN software, ...), matched case-insensitively against the adapter's
// friendly name (e.g. "vEthernet (WSL (Hyper-V firewall))").
static bool nameContainsVirtual(const WCHAR* name)
{
	static constexpr auto markers = std::to_array<std::wstring_view>({
		L"wsl", L"vethernet", L"hyper-v", L"virtual", L"vmware",
		L"virtualbox", L"vbox", L"tailscale", L"zerotier",
		L"docker", L"tap-", L"isatap", L"teredo"
	});
	std::wstring lower;
	for (auto c : std::wstring(name)) {
		lower.push_back(std::towlower(c));
	}
	return std::ranges::any_of(markers, [&](const auto& m) {
		return lower.find(m) != std::wstring::npos;
	});
}

// Preference score for a network adapter: wired > wireless > other
// physical > virtual (tunnels, Hyper-V/WSL switch NICs, ...). Negative
// scores are never used; virtual adapters are kept as a fallback so
// machines that only have one (e.g. WSL-only) still work.
static int adapterScore(const IP_ADAPTER_ADDRESSES& a)
{
	if (a.OperStatus != IfOperStatusUp) return -1;
	if (a.IfType == IF_TYPE_SOFTWARE_LOOPBACK) return -1;
	if (a.IfType == IF_TYPE_TUNNEL || a.TunnelType != TUNNEL_TYPE_NONE) {
		return 0; // tunnel / VPN
	}
	if (nameContainsVirtual(a.FriendlyName)) return 0;
	if (a.IfType == IF_TYPE_ETHERNET_CSMACD) return 3; // wired
	if (a.IfType == IF_TYPE_IEEE80211) return 2;       // wireless
	return 1; // other physical (PPP, ...)
}

// Fills the host info from the first usable IPv4 address of the given
// adapter (loopback excluded); nullopt when it has none.
static std::optional<HostNetInfo> hostNetInfoFromAdapter(const IP_ADAPTER_ADDRESSES& a)
{
	HostNetInfo info;
	for (auto* u = a.FirstUnicastAddress; u; u = u->Next) {
		auto* sa = std::bit_cast<const sockaddr_in*>(u->Address.lpSockaddr);
		if (sa->sin_family != AF_INET) continue;
		uint32_t ip = ntohl(sa->sin_addr.s_addr);
		if (ip == 0 || (ip & 0xFF000000) == 0x7F000000) continue; // skip loopback
		info.ip = ip;
		uint8_t prefix = u->OnLinkPrefixLength;
		info.netmask = (prefix == 0) ? 0 : (0xFFFFFFFFu << (32 - prefix));
		if (a.FirstGatewayAddress) {
			auto* g = std::bit_cast<const sockaddr_in*>(a.FirstGatewayAddress->Address.lpSockaddr);
			info.gateway = ntohl(g->sin_addr.s_addr);
		}
		int n = 0;
		for (auto* d = a.FirstDnsServerAddress; d && n < 2; d = d->Next) {
			auto* dsa = std::bit_cast<const sockaddr_in*>(d->Address.lpSockaddr);
			uint32_t dns = ntohl(dsa->sin_addr.s_addr);
			auto* dnsSlot = (n == 0) ? &info.dns1 : &info.dns2;
			*dnsSlot = dns;
			++n;
		}
		return info;
	}
	return std::nullopt;
}

static std::optional<HostNetInfo> getHostNetInfo()
{
	ULONG size = 0;
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
	                         nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) {
		return std::nullopt;
	}
	if (size == 0) return std::nullopt;
	auto buffer = std::make_unique_for_overwrite<uint8_t[]>(size);
	auto* adapters = std::bit_cast<PIP_ADAPTER_ADDRESSES>(buffer.get());
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
	                         nullptr, adapters, &size) != NO_ERROR) {
		return std::nullopt;
	}

	// Pick the preferred adapter: the highest-scoring one with a usable
	// IPv4 address (ties keep the first one).
	int bestScore = -1;
	std::optional<HostNetInfo> bestInfo;
	for (auto* a = adapters; a; a = a->Next) {
		int score = adapterScore(*a);
		if (score < 0 || score < bestScore) continue;
		auto info = hostNetInfoFromAdapter(*a);
		if (!info) continue;
		if (score > bestScore) {
			bestScore = score;
			bestInfo = std::move(info);
		}
	}
	return bestInfo;
}
#else
// Default gateway (Linux): /proc/net/route, first default (00000000)
// route. The address is printed as a little-endian hex value.
static void readDefaultGateway(HostNetInfo& info)
{
	std::ifstream route("/proc/net/route");
	std::string line;
	while (std::getline(route, line)) {
		if (line.starts_with("Iface")) continue; // header
		std::istringstream iss(line);
		std::string iface;
		std::string dest;
		std::string gw;
		std::string flags;
		iss >> iface >> dest >> gw >> flags;
		if (dest != "00000000") continue; // not the default route
		if (auto f = StringOp::stringToBase<16, unsigned long>(flags);
		    !f.has_value() || !(*f & 1)) continue;
		auto g = StringOp::stringToBase<16, unsigned long>(gw);
		if (!g.has_value()) continue;
		info.gateway = ntohl(*g);
		break;
	}
}

// Preference score for a network interface: wired > wireless > other
// physical > virtual (docker, veth, tun/tap, VPN, ...). Negative scores
// are never used; virtual interfaces are kept as a fallback so machines
// that only have one still work.
static int interfaceScore(std::string_view name, unsigned flags)
{
	if (!(flags & IFF_UP)) return -1;
	if (flags & IFF_LOOPBACK) return -1;
	static constexpr auto virtualPrefixes = std::to_array<std::string_view>({
		"docker", "veth", "virbr", "br-", "tun", "tap",
		"utun", "awdl", "bridge", "tailscale", "zt", "wg",
		"vmnet", "vbox"
	});
	for (auto& prefix : virtualPrefixes) {
		if (name.starts_with(prefix)) return 0;
	}
	if (name.starts_with("en") || name.starts_with("eth")) return 3; // wired
	if (name.starts_with("wl") || name.starts_with("wlan") ||
	    name.starts_with("ra")) return 2; // wireless
	return 1; // other physical
}

static std::optional<HostNetInfo> getHostNetInfo()
{
	struct ifaddrs* ifa = nullptr;
	if (getifaddrs(&ifa) != 0) return std::nullopt;

	// Pick the preferred interface: the highest-scoring one with a usable
	// IPv4 address (ties keep the first one).
	int bestScore = -1;
	std::optional<HostNetInfo> bestInfo;
	for (auto* it = ifa; it; it = it->ifa_next) {
		if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
		int score = interfaceScore(it->ifa_name, it->ifa_flags);
		if (score < 0 || score < bestScore) continue;
		auto* sa = std::bit_cast<const sockaddr_in*>(it->ifa_addr);
		uint32_t ip = ntohl(sa->sin_addr.s_addr);
		if (ip == 0) continue;
		HostNetInfo info;
		info.ip = ip;
		if (it->ifa_netmask) {
			auto* sm = std::bit_cast<const sockaddr_in*>(it->ifa_netmask);
			info.netmask = ntohl(sm->sin_addr.s_addr);
		}
		if (score > bestScore) {
			bestScore = score;
			bestInfo = std::move(info);
		}
	}
	freeifaddrs(ifa);
	if (!bestInfo) return std::nullopt;

#ifndef __APPLE__
	readDefaultGateway(*bestInfo);
#endif

	readResolvConfDns(bestInfo->dns1, bestInfo->dns2);
	return bestInfo;
}
#endif

// Ephemeral port selection (spec 4.4.1): random port in 16384-32767.
// Device-local generator: openMSX's random_int() helpers share a global
// generator that is not thread-safe against the emulation thread, and
// this code runs on the ESP thread.
static uint16_t randomEphemeralPort()
{
	static std::mt19937 rng(std::random_device{}());
	static std::uniform_int_distribution dist(16384, 32767);
	return static_cast<uint16_t>(dist(rng));
}

// ---- connection structure ----

// Passive TCP listener registry entry: multiple passive connections may
// share a listener for the same local port; the socket is owned by the
// registry and closed when the last referencing connection is freed.
struct EmulatedEsp32::ListenSocket {
	SOCKET sock = OPENMSX_INVALID_SOCKET;
	uint16_t port = 0;
	int refs = 0;
};

class EmulatedEsp32::Connection {
public:
	// The enclosing class manages connections (spawning/joining the
	// reader threads and tearing down sockets), so it needs access to
	// all state.
	friend class EmulatedEsp32;

	// Received-data buffer, guarded by recvMutex: all access goes through
	// the methods below.
	void pushRecv(uint8_t b) {
		std::scoped_lock lock(recvMutex);
		recvBuffer.push_back(b);
	}
	[[nodiscard]] size_t recvAvail() const {
		std::scoped_lock lock(recvMutex);
		return recvBuffer.size();
	}
	void popRecv(std::vector<uint8_t>& out, size_t max) {
		std::scoped_lock lock(recvMutex);
		size_t toRead = std::min(recvBuffer.size(), max);
		out.reserve(2 + toRead);
		for (size_t i = 0; i < toRead; ++i) {
			out.push_back(recvBuffer.front());
			recvBuffer.pop_front();
		}
	}
	void clearRecv() {
		std::scoped_lock lock(recvMutex);
		recvBuffer.clear();
	}

private:
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
	std::optional<OpenSSL::SessionHandle> ssl;  // null when not using TLS
	bool tlsVerify = false;     // validate the server certificate
	std::atomic<uint8_t> handshakePhase{0}; // 0=no TLS, 1=handshaking, 2=done

	std::unique_ptr<std::thread> readerThread;
	std::unique_ptr<Poller> poller;
	std::atomic<bool> readerActive{false};

	cb_queue<uint8_t> recvBuffer;
	mutable std::mutex recvMutex;
};

// ====================================================================
//  Construction / destruction
// ====================================================================

EmulatedEsp32::EmulatedEsp32(const DeviceConfig& config, std::function<void(uint8_t)> sink_)
	: enabledSetting(
		config.getCommandController(), "smxwifi-emulatedesp32-link-enabled",
		"Report the emulated ESP32 WiFi link as up (UNAPI NET_STATE)", true)
	, sink(std::move(sink_))
{
	// Probe for a host-installed OpenSSL runtime; TLS support is
	// advertised and enabled only when one is found (the TLS capability
	// bits are not advertised otherwise, and TLS TCP_OPEN requests get
	// ERR_NOT_IMP).
	if (const auto* lib = OpenSSL::load()) {
		config.getCliComm().printInfo(
			"SMXWiFi: TLS support enabled (",
			lib->version(), ')');
	} else {
		config.getCliComm().printInfo(
			"SMXWiFi: OpenSSL not found, TLS support disabled "
			"(install OpenSSL to enable it)");
	}

	// Start the ESP emulation thread immediately.  Note that we can NOT
	// rely on powerUp() to do this: when the extension is inserted into an
	// already-running machine, powerUp() is never called on the new device.
	start();
}

EmulatedEsp32::~EmulatedEsp32()
{
	stop();
}

// ====================================================================
//  Reset
// ====================================================================

// Closes all network connections and clears the emulated device state
// (parser, boot sequence and pending input). Called by the owner on MSX
// reset and whenever it switches to or from this endpoint, so the
// emulation always starts clean after a migration.
void EmulatedEsp32::resetState()
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
		cp->ssl.reset();
	}
			if (cp->sock != OPENMSX_INVALID_SOCKET) {
				sock_close(cp->sock);
			}
			cp.reset();
		}
	}
	resetFifo();
	lastResponse.clear();
	// Wake the thread so a pending "Ready" boot retry is cancelled
	// promptly (resetParser() clears the boot stage).
	std::scoped_lock lock(msxToEspMutex);
	msxToEspCond.notify_one();
}

// ====================================================================
//  Serial contract (the owner routes to this endpoint when active)
// ====================================================================

void EmulatedEsp32::recvByte(uint8_t value, EmuTime /*time*/)
{
	std::scoped_lock lock(msxToEspMutex);
	msxToEspFifo.push_back(value);
	msxToEspCond.notify_one();
}

void EmulatedEsp32::setBaudRate(unsigned /*baud*/) const
{
	// The emulated firmware does not care about the UART baud rate.
}

// Clears the pending MSX->ESP input (UART FIFO reset command).
void EmulatedEsp32::resetFifo()
{
	std::scoped_lock lock(msxToEspMutex);
	msxToEspFifo.clear();
	msxToEspCond.notify_one();
}

// ====================================================================
//  Thread helpers
// ====================================================================

void EmulatedEsp32::start()
{
	if (espRunning.load()) return;
	espRunning = true;
	espThread = std::thread([this] { espThreadFunc(); });
}

void EmulatedEsp32::stop()
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
		for (const auto& cp : connections) {
			if (!cp) continue;
			cp->readerActive = false;
			if (cp->poller) cp->poller->abort();
			if (cp->readerThread && cp->readerThread->joinable()) {
				cp->readerThread->join();
			}
			cp->readerThread.reset();
			cp->poller.reset();
	if (cp->ssl) {
		cp->ssl.reset();
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

// Boot sequence text to the MSX side (greeting, "Ready"): delivered
// byte-by-byte through the sink, like RS232Raw delivers bytes from the
// real ESP32 through the connector.
void EmulatedEsp32::pushBootText(std::string_view text) const
{
	for (char c : text) {
		sink(static_cast<uint8_t>(c));
	}
}

void EmulatedEsp32::resetParser()
{
	parserState = ParserState::IDLE;
	parserSizeStep = 0;
	parserExpectedSize = 0;
	parserDataBuf.clear();
}

// Dispatch a complete command. Must be called without msxToEspMutex held.
void EmulatedEsp32::processCommand()
{
	if (parserCmdByte < 63 || parserCmdByte >= 0x80) {
		handleUnapiCommand(parserCmdByte, parserDataBuf);
	} else {
		handleCustomCommand(parserCmdByte, parserDataBuf);
	}
	resetParser();
}

// Wait for work (FIFO data or shutdown) while the MSX->ESP FIFO is empty.
// Returns true when the thread should stop; on return 'unlocked' may hold
// work that must run without the mutex held.
bool EmulatedEsp32::espWaitForWork(std::unique_lock<std::mutex>& lock,
                                  std::function<void()>& unlocked)
{
	if (parserState == ParserState::IDLE) {
		if (bootStage == BootStage::NONE) {
			msxToEspCond.wait(lock, [this] {
				return !msxToEspFifo.empty() || !espRunning.load();
			});
			return !espRunning.load();
		}
		// "Ready" loop in progress: wake up on data, on shutdown, or when
		// the boot stage was cancelled (e.g. resetState() migrated the
		// endpoint away — a pending "Ready" retry must not fire after the
		// switch).
		bool timedOut = !msxToEspCond.wait_until(
			lock, bootEventTime, [this] {
			return !msxToEspFifo.empty() || !espRunning.load() ||
			       bootStage == BootStage::NONE;
		});
		if (!espRunning.load()) return true;
		if (timedOut) {
			unlocked = [this] { pushBootText("Ready\r\n"); };
			if (--bootReadyRetries == 0) {
				bootStage = BootStage::NONE;
			}
			bootEventTime = std::chrono::steady_clock::now() +
			                std::chrono::seconds(5);
		}
		return false;
	}

	// Waiting for the rest of a framed command: apply the firmware's
	// 250 ms inter-byte timeout.
	bool timedOut = !msxToEspCond.wait_until(
		lock, parserDeadline, [this] {
		return !msxToEspFifo.empty() || !espRunning.load();
	});
	if (!espRunning.load()) return true;
	if (timedOut) {
		resetParser();
	}
	return false;
}

// Parser state machine for one received byte, mirroring
// received_data_parser() in the ESP32 UNAPI firmware:
//  - unknown command bytes are discarded and the parser stays IDLE
//  - known quick commands execute immediately (no size header)
//  - known data commands expect CMD + 2-byte size (MSB first) + data
//  - a partial command not completed within 250 ms (no new byte
//    arriving) is discarded and the parser reverts to IDLE
// On return 'unlocked' may hold work that must run without the mutex held.
void EmulatedEsp32::espParseByte(uint8_t b, std::function<void()>& unlocked)
{
	switch (parserState) {
	case ParserState::IDLE:
		// Any byte received during the boot sequence stops it
		bootStage = BootStage::NONE;
		if (isQuickCustomCommand(b)) {
			// Quick custom command, no size header
			unlocked = [this, b] { handleCustomCommand(b, {}); };
			return;
		}
		if (isDataCommand(b)) {
			parserCmdByte = b;
			parserState = ParserState::WAIT_DATA_SIZE;
		}
		// Unknown command byte: discard, stay IDLE
		return;

	case ParserState::WAIT_DATA_SIZE:
		// 2-byte size, MSB first
		if (parserSizeStep == 0) {
			parserExpectedSize = static_cast<uint16_t>(b << 8);
			parserSizeStep = 1;
		} else {
			parserExpectedSize |= b;
			if (parserExpectedSize > MAX_CMD_DATA_LEN) {
				resetParser(); // invalid size, discard
			} else if (parserExpectedSize == 0) {
				// Size 0: process immediately, no data block
				unlocked = [this] { processCommand(); };
			} else {
				parserDataBuf.clear();
				parserState = ParserState::GET_DATA;
			}
			parserSizeStep = 0;
		}
		return;

	case ParserState::GET_DATA:
		parserDataBuf.push_back(b);
		if (parserDataBuf.size() >= parserExpectedSize) {
			unlocked = [this] { processCommand(); };
		}
		return;
	}
}

void EmulatedEsp32::espThreadFunc()
{
	// The parser and boot-sequence state used to be function-local; reset
	// it so a (re-)started thread begins from a clean state.
	resetParser();
	bootStage = BootStage::NONE;
	bootReadyRetries = 0;

	while (espRunning.load()) {
		// Work that must run without the mutex (command dispatch and
		// boot text pushes): decided under the lock, executed below.
		std::function<void()> unlocked;
		bool stop = false;
		{
			std::unique_lock lock(msxToEspMutex);

			if (bootStartPending) {
				// A reset was requested (cmdReset just ran): discard any
				// pending input — the real device loses its serial buffer
				// on reboot — and start the boot sequence (greeting
				// immediately after "R0").
				bootStartPending = false;
				resetParser();
				msxToEspFifo.clear();
				unlocked = [this] { pushBootText(bootGreeting); };
				bootStage = BootStage::READY_LOOP;
				bootReadyRetries = 3;
				bootEventTime = std::chrono::steady_clock::now() +
				                std::chrono::seconds(5);
			} else if (msxToEspFifo.empty()) {
				stop = espWaitForWork(lock, unlocked);
			} else {
				uint8_t b = msxToEspFifo.front();
				msxToEspFifo.pop_front();
				parserDeadline = std::chrono::steady_clock::now() +
				                 std::chrono::milliseconds(250);
				espParseByte(b, unlocked);
			}
		}
		if (stop) break;
		if (unlocked) unlocked();
	}
}

// ====================================================================
//  TCP reader thread  (reads inbound data from a connected TCP socket)
// ====================================================================

void EmulatedEsp32::tcpReaderThreadFunc(int connIdx)
{
	if (connIdx < 0 || connIdx >= MAX_CONNECTIONS) return;

	Connection* conn = nullptr;
	{
		std::scoped_lock lock(connectionsMutex);
		conn = connections[connIdx].get();
		if (!conn) return;
	}

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
	while (conn->readerActive.load()) {
		if (!tcpPollConnection(conn, connIdx)) break;
	}
}

// Polls the connection's sockets (never blocks) and handles whatever is
// ready. Returns false when the reader loop must stop (the remote side
// closed the connection, or TLS failed).
bool EmulatedEsp32::tcpPollConnection(Connection* conn, int connIdx)
{
	fd_set rfds;
	fd_set wfds;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	SOCKET s = (conn->listenSock != OPENMSX_INVALID_SOCKET)
	         ? conn->listenSock : conn->sock;
	bool inHandshake = (conn->handshakePhase.load() == 1);
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
	if (int sel = select(0, &rfds,
	                     inHandshake ? &wfds : nullptr, nullptr, &tv);
	    sel <= 0) {
#else
	auto maxFd = static_cast<int>(s);
	if (haveClient) {
		maxFd = std::max(maxFd, static_cast<int>(conn->clientSock));
	}
	if (int sel = select(maxFd + 1, &rfds,
	                     inHandshake ? &wfds : nullptr, nullptr, &tv);
	    sel <= 0) {
#endif
		// Nothing to read yet (or transient select error); re-check the
		// shutdown flag and try again
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return true;
	}
	if (FD_ISSET(s, &rfds) || (inHandshake && FD_ISSET(s, &wfds))) {
		if (conn->listenSock != OPENMSX_INVALID_SOCKET) {
			tcpAcceptClient(conn);
		} else if (inHandshake) {
			tcpDriveHandshake(conn, connIdx);
		} else if (tcpReadData(conn, connIdx)) {
			// Remote side closed the connection or TLS failed:
			// stop polling this connection.
			return false;
		}
	}
	if (haveClient && FD_ISSET(conn->clientSock, &rfds)) {
		tcpReadClient(conn);
	}
	if (conn->state == 4) {
		// TLS handshake or read failed: stop polling this connection
		// (the failure is reported by TCP_STATE via closeReason).
		return false;
	}
	return true;
}

// Passive: a connection request is pending on the (possibly shared)
// listener. Accept and attach it if none is attached yet; otherwise drop
// it immediately (firmware behavior).
void EmulatedEsp32::tcpAcceptClient(Connection* conn) const
{
	SOCKET ns = accept(conn->listenSock, nullptr, nullptr);
	if (ns == OPENMSX_INVALID_SOCKET) return;
	if (conn->clientSock != OPENMSX_INVALID_SOCKET) {
		// A client is already attached: drop the pending connection
		sock_close(ns);
		return;
	}
	// No client attached yet: attach this one
	sock_setNonBlocking(ns);
	// macOS/BSD only: suppress SIGPIPE when sending to a peer that reset
	// the connection (Linux uses MSG_NOSIGNAL instead). Not inherited by
	// accept().
#ifdef SO_NOSIGPIPE
	sock_setIntOption(ns, SOL_SOCKET, SO_NOSIGPIPE);
#endif
	conn->clientSock = ns;
	conn->clientEof = false;
	struct sockaddr_in peer{};
	if (::socklen_t peerLen = sizeof(peer);
	    getpeername(ns,
	        std::bit_cast<struct sockaddr*>(&peer),
	        &peerLen) == 0) {
		conn->remoteIP = ntohl(peer.sin_addr.s_addr);
		conn->remotePort = ntohs(peer.sin_port);
	}
	conn->state = 2; // open -> ESTABLISHED
}

// Active TLS: drive the handshake (the connection reports SYN-SENT until
// the handshake and certificate validation have finished).
void EmulatedEsp32::tcpDriveHandshake(Connection* conn, int connIdx)
{
	int r = conn->ssl->handshake();
	if (r == 1) {
		// Handshake finished: validate the certificate before the
		// connection becomes ESTABLISHED
		uint8_t reason = conn->tlsVerify
			? static_cast<uint8_t>(conn->ssl->verifyResult())
			: 0;
		if (reason != 0) {
			tcpTlsFail(conn, connIdx, reason);
		} else {
			conn->handshakePhase.store(2);
			conn->state = 2; // open -> ESTABLISHED
		}
	} else if (r < 0) {
		// Handshake failed (protocol error, peer reset, ...): report a
		// TLS error like the firmware (reason 19)
		tcpTlsFail(conn, connIdx, 19);
	}
	// r == 0 or 2: still waiting for read/write readiness
}

// Records a TLS failure: the close reason is sequenced before the state
// store, so TCP_STATE sees it whenever it observes state==4.
void EmulatedEsp32::tcpTlsFail(Connection* conn, int connIdx, uint8_t reason)
{
	closeReason[connIdx].store(reason);
	conn->state = 4; // failed (TLS) -> TCP_STATE reports ERR_NO_CONN
}

// Active: read inbound data. Returns true when the reader loop must stop
// (the remote side closed the connection, or TLS failed).
bool EmulatedEsp32::tcpReadData(Connection* conn, int connIdx)
{
	std::array<char, 1024> buf;
	ptrdiff_t n = 0;
	if (conn->ssl) {
		// TLS: decrypt; a clean TLS close is a close_notify, a real
		// error fails the connection, WouldBlock is just "try again
		// later".
		auto r = conn->ssl->read(
			std::span<char>(buf.data(), buf.size()));
		if (!r.has_value()) {
			if (r.error() == OpenSSL::IoError::Closed) {
				conn->state = 3; // remote closed -> CLOSE_WAIT
				return true;
			}
			if (r.error() == OpenSSL::IoError::Failed) {
				tcpTlsFail(conn, connIdx, 19); // TLS: other error
				return true;
			}
			// WouldBlock: nothing to do, loop again
			return false;
		}
		n = *r;
	} else {
		n = sock_recv(conn->sock, buf.data(), buf.size());
		if (n < 0) {
			// Socket closed by the remote side (EOF) or error
			conn->state = 3; // remote closed -> CLOSE_WAIT
			return true;
		}
	}
	if (n > 0) {
		{
			for (ptrdiff_t i = 0; i < n; ++i) {
				conn->pushRecv(buf[static_cast<size_t>(i)]);
			}
		}
		if (conn->ssl) {
			tcpDrainSsl(conn, connIdx);
		}
	}
	return false;
}

// The SSL layer may have more plaintext buffered than one SSL_read
// returns; drain it now (the socket itself would otherwise not be
// selectable again).
void EmulatedEsp32::tcpDrainSsl(Connection* conn, int connIdx)
{
	std::array<char, 1024> buf;
	for (;;) {
		auto r = conn->ssl->read(
			std::span<char>(buf.data(), buf.size()));
		if (!r.has_value()) {
			if (r.error() == OpenSSL::IoError::Closed) {
				conn->state = 3; // remote closed -> CLOSE_WAIT
			}
			if (r.error() == OpenSSL::IoError::Failed) {
				tcpTlsFail(conn, connIdx, 19);
			}
			// WouldBlock: nothing more to drain
			return;
		}
		for (ptrdiff_t i = 0; i < *r; ++i) {
			conn->pushRecv(buf[static_cast<size_t>(i)]);
		}
	}
}

// Accepted client socket: read inbound data. When the client closes the
// connection it stays attached (like the firmware's ClientList) and the
// connection reports CLOSE_WAIT; new pending connections keep being
// dropped.
void EmulatedEsp32::tcpReadClient(Connection* conn) const
{
	std::array<char, 1024> buf;
	auto n = sock_recv(conn->clientSock, buf.data(), buf.size());
	if (n < 0) {
		conn->clientEof = true;
		conn->state = 3; // remote closed -> CLOSE_WAIT
		return;
	}
	for (ptrdiff_t i = 0; i < n; ++i) {
		conn->pushRecv(buf[static_cast<size_t>(i)]);
	}
}

// ====================================================================
//  Connection manager
// ====================================================================

EmulatedEsp32::Connection* EmulatedEsp32::allocateConnection()
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (connections[i] && connections[i]->type != 0) continue;
		if (!connections[i]) {
			connections[i] = std::make_unique<Connection>();
		} else {
			resetConnectionSlot(i);
		}
		closeReason[i].store(0);
		return connections[i].get();
	}
	return nullptr;
}

// Resets a previously used connection slot to its fresh state.
void EmulatedEsp32::resetConnectionSlot(int i)
{
	connections[i]->type = 0;
	connections[i]->sock = OPENMSX_INVALID_SOCKET;
	connections[i]->listenSock = OPENMSX_INVALID_SOCKET;
	connections[i]->clientSock = OPENMSX_INVALID_SOCKET;
	connections[i]->listenIdx = -1;
	connections[i]->clientEof = false;
	connections[i]->clearRecv();
	connections[i]->remoteIP = 0;
	connections[i]->remotePort = 0;
	connections[i]->localPort = 0;
	connections[i]->flags = 0;
	connections[i]->state = 0;
	connections[i]->ssl.reset();
	connections[i]->tlsVerify = false;
	connections[i]->handshakePhase.store(0);
	connections[i]->readerActive = false;
	if (connections[i]->poller) connections[i]->poller->reset();
}

void EmulatedEsp32::releaseListenEntry(int idx)
{
	if (idx < 0 || idx >= static_cast<int>(listenSockets.size())) return;
	const auto& e = listenSockets[idx];
	if (--e->refs > 0) return; // still referenced by other connections
	sock_close(e->sock);
	listenSockets.erase(listenSockets.begin() + idx);
}

void EmulatedEsp32::freeConnection(int idx)
{
	if (idx < 0 || idx >= MAX_CONNECTIONS) return;
	const auto& cp = connections[idx];
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
		cp->ssl.reset();
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
		cp->handshakePhase.store(0);
	cp->tlsVerify = false;
	cp->type = 0;
	cp->state = 0;
	cp->clientEof = false;
	cp->clearRecv();
}

EmulatedEsp32::Connection* EmulatedEsp32::getConnection(int idx) const
{
	if (idx < 0 || idx >= MAX_CONNECTIONS) return nullptr;
	return connections[idx].get();
}

int EmulatedEsp32::getFreeConnectionSlot() const
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (!connections[i] || connections[i]->type == 0) return i;
	}
	return -1;
}

// ====================================================================
//  Response helpers
// ====================================================================

void EmulatedEsp32::saveLastResponse(std::span<const uint8_t> data)
{
	lastResponse.assign(data.begin(), data.end());
}

void EmulatedEsp32::sendQuickResponse(uint8_t cmdByte, uint8_t errorCode)
{
	std::array<uint8_t, 2> resp = {cmdByte, errorCode};
	saveLastResponse(resp);
	sink(cmdByte);
	sink(errorCode);
}

void EmulatedEsp32::sendResponse(uint8_t cmdByte, uint8_t errorCode,
                                std::span<const uint8_t> data)
{
	auto respSize = narrow<uint16_t>(data.size());
	auto resp = concat(std::to_array<uint8_t>({cmdByte, errorCode,
	                                           uint8_t(respSize >> 8),
	                                           uint8_t(respSize)}),
	                   data);

	saveLastResponse(resp);
	for (uint8_t b : resp) {
		sink(b);
	}
}

void EmulatedEsp32::sendRawResponse(std::span<const uint8_t> data)
{
	saveLastResponse(data);
	for (uint8_t b : data) {
		sink(b);
	}
}

// ====================================================================
//  Command dispatch
// ====================================================================

void EmulatedEsp32::handleCustomCommand(uint8_t cmd, std::span<const uint8_t> data)
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
	case 'Q': cmdGetSettings(data); break;
	case 'c': case 'C': cmdAutoClock(cmd, data); break;
	case 'T': cmdSetWiFiTimer(data); break;
	case 'G': cmdGetDateTime(data); break;
	default:
		sendQuickResponse(cmd, 4); // ERR_INV_PARAM
		break;
	}
}

void EmulatedEsp32::handleUnapiCommand(uint8_t cmd, std::span<const uint8_t> data)
{
	// Unimplemented command codes of the UNAPI spec: handled before the
	// switch to keep its case count low.
	if (cmd == 4 || cmd == 5 || cmd == 7 ||
	    (cmd >= 19 && cmd <= 24) || (cmd >= 27 && cmd <= 29)) {
		unimplementedCmd(cmd);
		return;
	}
	switch (cmd) {
	case 0:  cmdGetInfo(data); break;
	case 1:  cmdGetCapab(data); break;
	case 2:  cmdGetIPInfo(data); break;
	case 3:  cmdNetState(data); break;
	case 6:  cmdDnsQ(data); break;
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
	case 25: cmdCfgAutoIP(data); break;
	case 26: cmdCfgIP(data); break;
	case 206: cmdDnsQNew(data); break;
	default:
		unimplementedCmd(cmd);
		break;
	}
}

// ====================================================================
//  Helper: parse IP address string → 4 bytes
// ====================================================================

bool EmulatedEsp32::parseIP(const std::string& str, std::span<uint8_t, 4> ipOut) const
{
	uint32_t ip;
	if (!parseIPv4String(str, ip)) return false;
	Endian::write_UA_B32(ipOut.data(), ip);
	return true;
}

// ====================================================================
//  Custom command implementations
// ====================================================================

void EmulatedEsp32::cmdReset(std::span<const uint8_t> /*data*/)
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

void EmulatedEsp32::cmdWarmReset(std::span<const uint8_t> /*data*/)
{
	{
		std::scoped_lock lock(connectionsMutex);
		for (int i = 0; i < MAX_CONNECTIONS; ++i) {
			freeConnection(i);
		}
	}
	lastResponse.clear();
	// Raw text response, exactly like the firmware's Serial.println("Ready")
	static constexpr auto msg = std::to_array<uint8_t>({'R', 'e', 'a', 'd', 'y', '\r', '\n'});
	sendRawResponse(msg);
}

void EmulatedEsp32::cmdQuery(std::span<const uint8_t> /*data*/)
{
	// Raw text response (no framing — the MSX expects "OK")
	static constexpr auto ok = std::to_array<uint8_t>({'O', 'K'});
	sendRawResponse(ok);
}

void EmulatedEsp32::cmdGetVersion(std::span<const uint8_t> /*data*/)
{
	// Raw bytes, no framing: 'V' + major + minor as HEX values (not
	// ASCII) — the firmware writes chVer[0]-'0' and chVer[2]-'0'
	static constexpr auto resp = std::to_array<uint8_t>({'V', 0x01, 0x00});
	sendRawResponse(resp);
}

void EmulatedEsp32::cmdRetry(std::span<const uint8_t> /*data*/)
{
	if (lastResponse.empty()) {
		sendQuickResponse('r', 4); // no previous response
		return;
	}
	for (uint8_t b : lastResponse) {
		sink(b);
	}
}

void EmulatedEsp32::cmdScanAP(std::span<const uint8_t> /*data*/)
{
	sendQuickResponse('S', 0);
}

void EmulatedEsp32::cmdScanResults(std::span<const uint8_t> /*data*/)
{
	constexpr uint8_t SCAN_NO_NETWORKS = 2;
	sendQuickResponse('s', SCAN_NO_NETWORKS);
}

void EmulatedEsp32::cmdConnectAP(std::span<const uint8_t> /*data*/)
{
	constexpr uint8_t ERR_OK = 0;
	sendQuickResponse('A', ERR_OK);
}

void EmulatedEsp32::cmdGetAPStatus(std::span<const uint8_t> /*data*/)
{
	static constexpr uint8_t statusConnected = 5; // Connected and got IP
	static constexpr zstring_view apName = "SMXWiFi";
	auto apNameLen = narrow<uint8_t>(apName.size());
	uint16_t respSize = 1 + apNameLen + 1; // status + name + null

	std::vector<uint8_t> resp;
	resp.reserve(respSize);
	resp.push_back(statusConnected);
	resp.insert(resp.end(), apName.begin(), apName.end());
	resp.push_back(0); // null terminator

	sendResponse('g', 0, resp);
}

void EmulatedEsp32::cmdFirmwareUpdate(uint8_t cmd, std::span<const uint8_t> /*data*/)
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

void EmulatedEsp32::cmdGetSettings(std::span<const uint8_t> /*data*/)
{
	// Framed response, like the firmware's
	// SendResponse(CUSTOM_F_QUERY_SETTINGS, OK, len, "OFF:30")
	static constexpr auto settings = std::to_array<uint8_t>({
		'O', 'F', 'F', ':', '3', '0'
	});
	sendResponse('Q', 0, settings);
}

void EmulatedEsp32::cmdAutoClock(uint8_t cmd, std::span<const uint8_t> data)
{
	if (cmd == 'c') {
		// AutoClock=0 (off), GMT=0
		std::array<uint8_t, 3> resp = {0, 0, 0};
		sendResponse('c', 0, resp);
		return;
	}
	// data: autoClock (0..3) + GMT offset byte
	if (data.size() != 2 || data[0] > 3) {
		sendQuickResponse('C', 4); // UNAPI_ERR_INV_PARAM
		return;
	}
	sendQuickResponse('C', 0);
}

void EmulatedEsp32::cmdSetWiFiTimer(std::span<const uint8_t> data)
{
	// data: radio-off timer, 2 bytes (MSB first)
	if (data.size() != 2) {
		sendQuickResponse('T', 4); // UNAPI_ERR_INV_PARAM
		return;
	}
	sendQuickResponse('T', 0);
}

void EmulatedEsp32::cmdGetDateTime(std::span<const uint8_t> /*data*/)
{
	// Return "no network" error since SNTP is not implemented
	constexpr uint8_t ERR_NO_NETWORK = 2;
	sendQuickResponse('G', ERR_NO_NETWORK);
}

// ====================================================================
//  UNAPI command implementations
// ====================================================================

void EmulatedEsp32::cmdGetInfo(std::span<const uint8_t> /*data*/)
{
	// UNAPI_GET_INFO — not normally sent over serial.
	// Return ERR_NOT_IMP as the MSX driver handles this locally.
	unimplementedCmd(0);
}

void EmulatedEsp32::unimplementedCmd(uint8_t cmd)
{
	// All UNAPI responses use the framed format (CMD, ERR, SIZE, data)
	sendResponse(cmd, 1); // ERR_NOT_IMP
}

void EmulatedEsp32::cmdGetCapab(std::span<const uint8_t> data)
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
		std::array<uint8_t, 5> resp = {0x2C, 0x44, 0x96, 0x1C, 0x04};
		sendResponse(1, 0, resp);
		break;
	}
	case 2: {
		// Max TCP (1), max UDP (1), free TCP (1), free UDP (1),
		// max raw TCP (1), max raw UDP (1)
		std::array<uint8_t, 6> resp = {4, 4, 0, 0, 0, 0};
		{
			std::scoped_lock lock(connectionsMutex);
			unsigned char used = 0;
			for (const auto& cp : connections) {
				if (cp && cp->type != 0) ++used;
			}
			resp[2] = 4 - used; // free TCP
			resp[3] = 4 - used; // free UDP
		}
		sendResponse(1, 0, resp);
		break;
	}
	case 3: {
		// Max incoming datagram size (2 LSB MSB), max outgoing (2 LSB MSB).
		// Compile-time constants left as explicit byte literals: the
		// endianness is visible in the initializer itself (no runtime
		// serialization to factor into Endian::write_UA_L16()).
		std::array<uint8_t, 4> resp = {
			1500 & 0xFF, (1500 >> 8) & 0xFF,
			2048 & 0xFF, (2048 >> 8) & 0xFF
		};
		sendResponse(1, 0, resp);
		break;
	}
	case 4: {
		// Secondary capability flags (2), feature flags (2), unused (1)
		// Bit 8 (use TLS in TCP active connections) is advertised only
		// when a host OpenSSL runtime is available; bit 9 (TLS in passive
		// connections) is never advertised.
		std::array<uint8_t, 5> resp = {0xF7, 0x00, 0, 0, 0};
		if (OpenSSL::load()) {
			resp[1] |= 0x01; // bit 8: TLS in TCP active connections
		}
		sendResponse(1, 0, resp);
		break;
	}
	default:
		break;
	}
}

void EmulatedEsp32::cmdGetIPInfo(std::span<const uint8_t> data)
{
	if (data.size() != 1 || data[0] == 0 || data[0] > 6) {
		sendResponse(2, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t idx = data[0];
	std::array<uint8_t, 4> ipBytes{};
	if (idx != 2) { // idx 2 = Peer IP, not applicable
		// Report the host's real network configuration (like the ESP32
		// reporting its WiFi IP); it is recomputed per query, so it tracks
		// host interface changes.
		uint32_t ip = 0;
		if (auto net = getHostNetInfo()) {
			switch (idx) {
			case 1: ip = net->ip; break;
			case 3: ip = net->netmask; break;
			case 4: ip = net->gateway; break;
			case 5: ip = net->dns1; break;
			case 6: ip = net->dns2; break;
			default: break;
			}
		}
		// No usable host value: 0.0.0.0 for IP/netmask/gateway/DNS2.
		// Primary DNS gets a mock (8.8.8.8): DNS lookups are performed on
		// the host anyway, so the value is purely informational.
		if (idx == 5 && ip == 0) ip = 0x08080808;
		Endian::write_UA_B32(ipBytes.data(), ip);
	}
	sendResponse(2, 0, ipBytes);
}

void EmulatedEsp32::cmdNetState(std::span<const uint8_t> data)
{
	if (!data.empty()) {
		sendResponse(3, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t state = enabledSetting.getBoolean() ? 2 : 0; // 2=Open, 0=Closed
	sendResponse(3, 0, {&state, 1});
}

void EmulatedEsp32::cmdDnsQ(std::span<const uint8_t> data)
{
	// data: zero-terminated host name string
	// Find null terminator
	if (data.empty()) {
		sendResponse(6, 4); // ERR_INV_PARAM
		return;
	}
	// Build null-terminated string
	std::string hostname(data.begin(), data.end());
	// Remove trailing null if present
	if (!hostname.empty() && hostname.back() == '\0') {
		hostname.pop_back();
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo* res = nullptr;
	if (int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
	    err != 0 || !res) {
		if (res) freeaddrinfo(res);
		sendResponse(6, 8); // ERR_DNS
		return;
	}

	auto* addr = std::bit_cast<const struct sockaddr_in*>(res->ai_addr);
	uint32_t ip = ntohl(addr->sin_addr.s_addr);
	std::array<uint8_t, 4> ipBytes;
	Endian::write_UA_B32(ipBytes.data(), ip);
	freeaddrinfo(res);

	sendResponse(6, 0, ipBytes);
}

void EmulatedEsp32::cmdDnsQNew(std::span<const uint8_t> data)
{
	// data[0] = flags (only bits 0-2 are defined), data[1..] =
	// zero-terminated host name string
	if (data.size() < 2 || (data[0] & 0xf8)) {
		sendResponse(206, 4); // ERR_INV_PARAM
		return;
	}
	uint8_t flags = data[0];
	auto hostnameSpan = data.subspan(1);
	std::string hostname(hostnameSpan.begin(), hostnameSpan.end());
	if (!hostname.empty() && hostname.back() == '\0') {
		hostname.pop_back();
	}

	// If the string is a valid IP address, use it directly
	if (std::array<uint8_t, 4> ipBytes; parseIP(hostname, ipBytes)) {
		sendResponse(206, 0, ipBytes);
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
	if (int err = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
	    err != 0 || !res) {
		if (res) freeaddrinfo(res);
		sendResponse(206, 8); // ERR_DNS
		return;
	}

	auto* addr = std::bit_cast<const struct sockaddr_in*>(res->ai_addr);
	uint32_t ipN = ntohl(addr->sin_addr.s_addr);
	std::array<uint8_t, 4> ipB;
	Endian::write_UA_B32(ipB.data(), ipN);
	freeaddrinfo(res);

	sendResponse(206, 0, ipB);
}

void EmulatedEsp32::cmdUdpOpen(std::span<const uint8_t> data)
{
	// Parameters: localPort (2, LSB MSB) + transient flag (1)
	if (data.size() != 3) {
		sendResponse(8, 4); // ERR_INV_PARAM
		return;
	}
	uint16_t localPort = Endian::read_UA_L16(data.data());
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
		auto udpPortInUse = [this](uint16_t port) {
			return std::ranges::any_of(connections, [&](const auto& cp) {
				return cp && cp->type == 2 && cp->localPort == port;
			});
		};
		do {
			localPort = randomEphemeralPort();
		} while (udpPortInUse(localPort));
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == OPENMSX_INVALID_SOCKET) {
		sendResponse(8, 2); // ERR_NO_NETWORK
		return;
	}

	// Bind to local port
	{
		sockaddr_in bindAddr = sock_makeIPv4(0, localPort);
		if (bind(s, std::bit_cast<struct sockaddr*>(&bindAddr),
		         sizeof(bindAddr)) < 0) {
			sock_close(s);
			sendResponse(8, 10); // ERR_CONN_EXISTS
			return;
		}
	}

	// Make socket non-blocking
	sock_setNonBlocking(s);

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
	auto connNum = static_cast<uint8_t>(connIdx + 1);
	sendResponse(8, 0, {&connNum, 1});
}

void EmulatedEsp32::cmdUdpClose(std::span<const uint8_t> data)
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
	if (const auto* conn = getConnection(num - 1);
	    !conn || conn->type != 2) {
		sendResponse(9, 11); // ERR_NO_CONN
		return;
	}
	freeConnection(num - 1);
	sendResponse(9, 0, {});
}

void EmulatedEsp32::cmdUdpState(std::span<const uint8_t> data)
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
	const auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 2) {
		sendResponse(10, 11); // ERR_NO_CONN
		return;
	}
	// Return: local port (2), pending datagrams (1), oldest size (2)
	uint16_t port = conn->localPort;
	// Count pending datagrams is tricky with raw sockets — just return 0
	uint8_t pendingDgrams = 0;
	uint16_t oldestSize = 0;
	// Built as a byte literal rather than Endian::write_UA_L16() calls:
	// three fields of mixed width in one initializer, where the explicit
	// byte order is self-documenting.
	std::array<uint8_t, 5> resp = {
		static_cast<uint8_t>(port & 0xFF),
		static_cast<uint8_t>((port >> 8) & 0xFF),
		pendingDgrams,
		static_cast<uint8_t>(oldestSize & 0xFF),
		static_cast<uint8_t>((oldestSize >> 8) & 0xFF)
	};
	sendResponse(10, 0, resp);
}

void EmulatedEsp32::cmdUdpSend(std::span<const uint8_t> data)
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
	uint16_t destPort = Endian::read_UA_L16(data.data() + 5);

	std::scoped_lock lock(connectionsMutex);
	const auto* conn = getConnection(num - 1);
	if (!conn) {
		sendResponse(11, 11); // ERR_NO_CONN
		return;
	}
	if (conn->type != 2) {
		sendResponse(11, 2); // ERR_NO_NETWORK (firmware quirk)
		return;
	}

	sockaddr_in dest = sock_makeIPv4(destIP, destPort);

	auto payload = data.subspan(7);
	if (int n = sendto(conn->sock,
	                   std::bit_cast<const char*>(payload.data()),
	                   static_cast<int>(payload.size()), 0,
	                   std::bit_cast<struct sockaddr*>(&dest), sizeof(dest));
	    n != static_cast<int>(payload.size())) {
		sendResponse(11, 2); // ERR_NO_NETWORK
		return;
	}
	sendResponse(11, 0, {});
}

void EmulatedEsp32::cmdUdpRcv(std::span<const uint8_t> data)
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
	uint16_t maxSize = Endian::read_UA_L16(data.data() + 1);
	if (maxSize > 2048) maxSize = 2048;

	std::scoped_lock lock(connectionsMutex);
	const Connection* conn = getConnection(num - 1);
	if (!conn || conn->type != 2) {
		sendResponse(12, 11); // ERR_NO_CONN
		return;
	}

	// Non-blocking receive
	std::vector<char> buf(maxSize > 0 ? maxSize : 2048);
	struct sockaddr_in from{};
	::socklen_t fromLen = sizeof(from);
	int n = recvfrom(conn->sock, buf.data(), static_cast<int>(buf.size()), 0,
	                 std::bit_cast<struct sockaddr*>(&from), &fromLen);
#ifdef _WIN32
	// Winsock reports a datagram larger than the buffer as an error
	// (WSAEMSGSIZE) after filling the buffer and consuming the datagram:
	// that is a truncate-at-receive, not a failure. (POSIX recvfrom just
	// fills the buffer.)
	if (n < 0 && WSAGetLastError() == WSAEMSGSIZE) {
		n = static_cast<int>(buf.size());
	}
#endif
	if (n < 0) {
		sendResponse(12, 3); // ERR_NO_DATA (includes 'would block')
		return;
	}

	uint32_t srcIP = ntohl(from.sin_addr.s_addr);
	uint16_t srcPort = ntohs(from.sin_port);

	// Response: remote IP (4) + remote port (2 LSB MSB) + data
	std::array<uint8_t, 6> hdr;
	Endian::write_UA_B32(hdr.data(), srcIP);
	Endian::write_UA_L16(hdr.data() + 4, srcPort);
	std::vector<uint8_t> resp;
	resp.reserve(6 + n);
	resp.insert(resp.end(), hdr.begin(), hdr.end());
	resp.insert(resp.end(), buf.begin(), buf.begin() + n);

	sendResponse(12, 0, resp);
}

void EmulatedEsp32::cmdTcpOpen(std::span<const uint8_t> data)
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
	uint16_t remotePort = Endian::read_UA_L16(data.data() + 4);
	uint16_t localPort = Endian::read_UA_L16(data.data() + 6);
	uint16_t userTimeout = Endian::read_UA_L16(data.data() + 8);
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
		hostname.assign(data.begin() + 11, data.begin() + 11 + len);
		if (auto nul = hostname.find('\0'); nul != std::string::npos) {
			hostname.resize(nul);
		}
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
		if (useTls && !OpenSSL::load()) {
			sendResponse(13, 1); // ERR_NOT_IMP
			return;
		}
	}
	// Flag bit 3 (verify certificate) is only meaningful with TLS on an
	// active connection; otherwise it is ignored (spec 4.5.1)

	// FFFFh local port: random port in 16384-32767, never a port number
	// used by another open TCP connection (spec 4.5.1)
	if (localPort == 0xFFFF) {
		pickEphemeralTcpPort(localPort);
	}

	if (passive) {
		cmdTcpOpenPassive(localPort, flags);
	} else {
		cmdTcpOpenActive(remoteIP, remotePort, localPort, flags,
		                 hostname, useTls, verifyCert);
	}
}

// Random ephemeral port (spec 4.5.1), never one used by another open
// TCP connection.
void EmulatedEsp32::pickEphemeralTcpPort(uint16_t& port)
{
	std::scoped_lock lock(connectionsMutex);
	auto tcpPortInUse = [this](uint16_t p) {
		return std::ranges::any_of(connections, [&](const auto& cp) {
			return cp && cp->type == 1 && cp->localPort == p;
		});
	};
	do {
		port = randomEphemeralPort();
	} while (tcpPortInUse(port));
}

// Passive open: find an existing shared listener for this port, or
// create a new one (passive connections with unspecified remote socket
// may share a local port, spec 4.5.1); then register the connection and
// start its reader thread.
void EmulatedEsp32::cmdTcpOpenPassive(uint16_t localPort, uint8_t flags)
{
	std::scoped_lock lock(connectionsMutex);

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
		sock_setIntOption(ls, SOL_SOCKET, SO_REUSEADDR);
		if (sockaddr_in bindAddr = sock_makeIPv4(0, localPort);
		    bind(ls, std::bit_cast<struct sockaddr*>(&bindAddr),
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
		sock_setNonBlocking(ls);
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

	auto connNum = static_cast<uint8_t>(connIdx + 1);
	sendResponse(13, 0, {&connNum, 1});
}

// Active open: check that no TCP connection is already open with the
// same combination of local port, remote IP and remote port (spec:
// ERR_CONN_EXISTS), and that no passive listener owns the port; then
// create the socket, connect, set up TLS and register the connection.
void EmulatedEsp32::cmdTcpOpenActive(uint32_t remoteIP, uint16_t remotePort,
                                     uint16_t localPort, uint8_t flags,
                                     const std::string& hostname,
                                     bool useTls, bool verifyCert)
{
	{
		std::scoped_lock lock(connectionsMutex);
		for (const auto& cp : connections) {
			if (cp && cp->type == 1 && cp->listenIdx < 0 &&
			    cp->localPort == localPort && cp->remoteIP == remoteIP &&
			    cp->remotePort == remotePort) {
				sendResponse(13, 10); // ERR_CONN_EXISTS
				return;
			}
		}
		for (const auto& e : listenSockets) {
			if (e->port == localPort) {
				sendResponse(13, 10); // ERR_CONN_EXISTS
				return;
			}
		}
		if (getFreeConnectionSlot() < 0) {
			sendResponse(13, 9); // ERR_NO_FREE_CONN
			return;
		}
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == OPENMSX_INVALID_SOCKET) {
		sendResponse(13, 2); // ERR_NO_NETWORK
		return;
	}

	// TCP_NODELAY
	sock_setIntOption(s, IPPROTO_TCP, TCP_NODELAY);
	// macOS/BSD only: suppress SIGPIPE when sending to a peer that reset
	// the connection (Linux uses MSG_NOSIGNAL instead).
#ifdef SO_NOSIGPIPE
	sock_setIntOption(s, SOL_SOCKET, SO_NOSIGPIPE);
#endif
	sock_setIntOption(s, SOL_SOCKET, SO_REUSEADDR);

	// Bind local port (may fail if e.g. an OS-level socket owns it)
	{
		sockaddr_in bindAddr = sock_makeIPv4(0, localPort);
		if (bind(s, std::bit_cast<struct sockaddr*>(&bindAddr),
		         sizeof(bindAddr)) < 0) {
			sock_close(s);
			sendResponse(13, 10); // ERR_CONN_EXISTS
			return;
		}
	}

	// Connect
	if (sockaddr_in dest = sock_makeIPv4(remoteIP, remotePort);
	    connect(s, std::bit_cast<struct sockaddr*>(&dest),
	            sizeof(dest)) < 0) {
		sock_close(s);
		// Return error with close reason byte (unknown reason)
		uint8_t reasonByte = 0;
		sendResponse(13, 11, {&reasonByte, 1}); // ERR_NO_CONN
		return;
	}

	// Make socket non-blocking (reader thread polls for data)
	sock_setNonBlocking(s);

	// TLS: create the OpenSSL session on the connected socket; the
	// handshake itself runs in the reader thread (the connection reports
	// SYN-SENT until it is finished, spec 4.5.5). The host name is used
	// for SNI and, when verifying, for the certificate host name check.
	std::optional<OpenSSL::SessionHandle> ssl;
	if (useTls) {
		ssl = OpenSSL::load()->createClientSession(
			verifyCert, hostname, static_cast<int>(s));
		if (!ssl) {
			sock_close(s);
			uint8_t reasonByte = 19; // TLS: other error
			sendResponse(13, 11, {&reasonByte, 1}); // ERR_NO_CONN
			return;
		}
	}

	{
		std::scoped_lock lock(connectionsMutex);
		auto* conn = allocateConnection();
		// A free slot was checked before connecting; since all commands
		// run on the same ESP thread, the slot is still free.
		if (!conn) {
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
		conn->ssl = std::move(ssl);
		conn->tlsVerify = verifyCert;
		conn->handshakePhase = conn->ssl ? 1 : 0; // 1 = TLS handshake in progress
		// Without TLS the TCP connect() already finished, so the
		// connection is ESTABLISHED right away. With TLS it reports
		// SYN-SENT (state=1) until the handshake and certificate
		// validation have completed (spec 4.5.5: TCP_STATE must not
		// report ESTABLISHED before the handshake is done).
		conn->state = conn->ssl ? 1 : 2;

		// Start reader thread
		conn->readerActive = true;
		conn->readerThread = std::make_unique<std::thread>(
		        [this, connIdx] { tcpReaderThreadFunc(connIdx); });

		auto connNum = static_cast<uint8_t>(connIdx + 1);
		sendResponse(13, 0, {&connNum, 1});
	}
}

void EmulatedEsp32::cmdTcpClose(std::span<const uint8_t> data)
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

void EmulatedEsp32::cmdTcpAbort(std::span<const uint8_t> data)
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

void EmulatedEsp32::cmdTcpState(std::span<const uint8_t> data)
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
	const auto* conn = getConnection(num - 1);
	if (!conn || conn->type != 1) {
		sendResponse(16, 11); // ERR_NO_CONN
		return;
	}

	// Connection failed (e.g. TLS handshake or certificate error): report
	// ERR_NO_CONN with the close reason stored when it failed (spec 4.5.4)
	if (conn->state == 4) {
		uint8_t reason = closeReason[num - 1].load();
		sendResponse(16, 11, {&reason, 1}); // ERR_NO_CONN
		return;
	}

	// Passive connection with no client attached yet: LISTEN state, only
	// the local port is meaningful (like the firmware's TcpState)
	if (conn->listenSock != OPENMSX_INVALID_SOCKET &&
	    conn->clientSock == OPENMSX_INVALID_SOCKET) {
		std::array<uint8_t, 16> resp{};
		resp[1] = 1; // LISTEN
		Endian::write_UA_L16(resp.data() + 14, conn->localPort);
		sendResponse(16, 0, resp);
		return;
	}
	// Deliberately not narrow<>(): recvBuffer is unbounded (the reader
	// thread keeps appending until the MSX drains it), so with a slow
	// or stopped MSX it can exceed 16-bit range; the field is 16-bit on
	// the wire, so it wraps like the firmware's bounded-buffer avail
	// would. Capping recvBuffer would make narrow<>() safe here.
	auto avail = static_cast<uint16_t>(conn->recvAvail());

	// Info block, same layout as the firmware's TcpState():
	// TLS flag (1) + lwIP state (1) + available (2 LSB MSB)
	// + urgent (2) + send space (2 LSB MSB, capped at 2048)
	// + remote IP (4) + remote port (2 LSB MSB) + local port (2 LSB MSB)
	std::array<uint8_t, 16> resp{};
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
	Endian::write_UA_L16(resp.data() + 2, avail);
	resp[4] = 0;                       // no urgent data
	resp[5] = 0;
	uint16_t sendSpace = 2048;         // arbitrary, capped like the firmware
	Endian::write_UA_L16(resp.data() + 6, sendSpace);
	Endian::write_UA_B32(resp.data() + 8, conn->remoteIP);
	Endian::write_UA_L16(resp.data() + 12, conn->remotePort);
	Endian::write_UA_L16(resp.data() + 14, conn->localPort);

	sendResponse(16, 0, resp);
}

void EmulatedEsp32::cmdTcpSend(std::span<const uint8_t> data)
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
		if (conn->handshakePhase.load() != 2) {
			sendResponse(17, 12); // ERR_CONN_STATE
			return;
		}
		auto r = conn->ssl->write(
			std::span<const char>(std::bit_cast<const char*>(payload.data()),
			                      payload.size()));
		if (!r.has_value()) {
			if (r.error() == OpenSSL::IoError::Closed) {
				// Peer sent close_notify: the connection is closed
				conn->state = 3; // remote closed -> CLOSE_WAIT
				sendResponse(17, 12); // ERR_CONN_STATE
				return;
			}
			// WouldBlock (send buffer full): report the retryable
			// ERR_BUFFER, like the plain-socket path does. A real TLS
			// error (Failed) keeps ERR_CONN_STATE. Note: SSL_write may
			// have partially written a record before blocking, so a
			// driver retrying the same payload can in rare cases
			// deliver it twice.
			sendResponse(17, (r.error() == OpenSSL::IoError::WouldBlock) ? 13 : 12);
			return;
		}
		n = *r;
	} else {
		n = sock_send(sendSock,
		              std::bit_cast<const char*>(payload.data()),
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

void EmulatedEsp32::cmdTcpRcv(std::span<const uint8_t> data)
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
	uint16_t maxSize = Endian::read_UA_L16(data.data() + 1);
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
	conn->popRecv(buf, maxSize);

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

void EmulatedEsp32::cmdCfgAutoIP(std::span<const uint8_t> data)
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

void EmulatedEsp32::cmdCfgIP(std::span<const uint8_t> data)
{
	// field (1, 1..6 except 2) + IP (4)
	if (data.size() != 5 || data[0] == 0 || data[0] == 2 || data[0] > 6) {
		sendResponse(26, 4); // ERR_INV_PARAM
		return;
	}
	// Just store in settings (optional) and return OK
	sendResponse(26, 0, {});
}

} // namespace openmsx

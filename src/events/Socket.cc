#include "Socket.hh"

#include "MSXException.hh" // FatalError
#include "utf8_checked.hh"

#include <bit>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace openmsx {

namespace {

// Parse a dotted-quad IPv4 string into a value in network byte order
// (0 on failure)
bool parseIPv4(const std::string& str, uint32_t& ipOut)
{
	unsigned a, b, c, d;
	if (std::sscanf(str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
		return false;
	}
	if (a > 255 || b > 255 || c > 255 || d > 255) return false;
	ipOut = (a << 24) | (b << 16) | (c << 8) | d;
	return true;
}

// Read the first two DNS servers from /etc/resolv.conf
void readResolvConfDns(uint32_t& dns1, uint32_t& dns2)
{
	std::ifstream f("/etc/resolv.conf");
	std::string line;
	int n = 0;
	while (n < 2 && std::getline(f, line)) {
		auto pos = line.find_first_not_of(" \t");
		if (pos == std::string::npos) continue;
		line = line.substr(pos);
		if (line.compare(0, 11, "nameserver ") != 0) continue;
		std::string ipStr = line.substr(11);
		auto hash = ipStr.find('#');
		if (hash != std::string::npos) ipStr.resize(hash);
		uint32_t ip = 0;
		if (parseIPv4(ipStr, ip)) {
			if (n == 0) {
				dns1 = ip;
			} else {
				dns2 = ip;
			}
			++n;
		}
	}
}

} // namespace

std::string sock_error()
{
#ifdef _WIN32
	wchar_t* s = nullptr;
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, WSAGetLastError(), 0, reinterpret_cast<LPWSTR>(&s),
		0, nullptr);
	std::string result = utf8::utf16to8(s);
	LocalFree(s);
	return result;
#else
	return strerror(errno);
#endif
}

void sock_startup()
{
#ifdef _WIN32
	// MAKEWORD is #define'd as ((WORD)(((BYTE)(a))|(((WORD)((BYTE)(b)))<<8)))
	// but using that gives old-style-cast warnings
	WORD w = 1 | (1 << 8); // MAKEWORD(1, 1)
	WSAData wsaData;
	if (WSAStartup(w, &wsaData) != 0) {
		throw FatalError(sock_error());
	}
#else
	// nothing needed for unix
#endif
}

void sock_cleanup()
{
#ifdef _WIN32
	WSACleanup();
#else
	// nothing needed for unix
#endif
}

// close a connection
void sock_close(SOCKET sd)
{
#ifdef _WIN32
	closesocket(sd);
#else
	close(sd);
#endif
}


ptrdiff_t sock_recv(SOCKET sd, char* buf, size_t count)
{
	ptrdiff_t num = recv(sd, buf, count, 0);
	if (num >  0) return num; // normal case
	if (num == 0) return -1;  // socket was closed by client
#ifdef _WIN32
	// Something bad happened on the socket.  It could just be a
	// "would block" notification, or it could be something more
	// serious.  WSAEWOULDBLOCK can happen after select() says a
	// socket is readable under Win9x, it doesn't happen on
	// WinNT/2000 or on Unix.
	int err;
	int errLen = sizeof(err);
	getsockopt(sd, SOL_SOCKET, SO_ERROR, std::bit_cast<char*>(&err), &errLen);
	if (err == WSAEWOULDBLOCK) return 0;
	return -1;
#else
	if (errno == EWOULDBLOCK) return 0;
	return -1;
#endif
}


ptrdiff_t sock_send(SOCKET sd, const char* buf, size_t count)
{
	ptrdiff_t num = send(sd, buf, count, 0);
	if (num >= 0) return num; // normal case
#ifdef _WIN32
	int err;
	int errLen = sizeof(err);
	getsockopt(sd, SOL_SOCKET, SO_ERROR, std::bit_cast<char*>(&err), &errLen);
	if (err == WSAEWOULDBLOCK) return 0;
	return -1;
#else
	if (errno == EWOULDBLOCK) return 0;
	return -1;
#endif
}

ptrdiff_t sock_sendto(SOCKET sd, const char* buf, size_t count, const struct sockaddr* to, socklen_t tolen)
{
	ptrdiff_t num = sendto(sd, buf, count, 0, to, tolen);
	if (num >= 0) return num;
#ifdef _WIN32
	return -1;
#else
	return -1;
#endif
}

ptrdiff_t sock_recvfrom(SOCKET sd, char* buf, size_t count, struct sockaddr* from, socklen_t* fromlen)
{
	ptrdiff_t num = recvfrom(sd, buf, count, 0, from, fromlen);
	if (num > 0) return num;
	if (num == 0) return -1;
#ifdef _WIN32
	int err;
	int errLen = sizeof(err);
	getsockopt(sd, SOL_SOCKET, SO_ERROR, std::bit_cast<char*>(&err), &errLen);
	if (err == WSAEWOULDBLOCK) return 0;
	return -1;
#else
	if (errno == EWOULDBLOCK) return 0;
	return -1;
#endif
}

#ifdef _WIN32
bool sock_get_net_info(SockNetInfo& info)
{
	ULONG size = 0;
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
	                         nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) {
		return false;
	}
	if (size == 0) return false;
	std::vector<unsigned char> buf(size);
	auto* adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
	                         nullptr, adapters, &size) != NO_ERROR) {
		return false;
	}
	for (auto* a = adapters; a; a = a->Next) {
		if (a->OperStatus != IfOperStatusUp) continue;
		for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
			auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
			if (sa->sin_family != AF_INET) continue;
			uint32_t ip = ntohl(sa->sin_addr.s_addr);
			if (ip == 0 || (ip & 0xFF000000) == 0x7F000000) continue; // skip loopback
			info.ip = ip;
			uint8_t prefix = u->OnLinkPrefixLength;
			info.netmask = (prefix == 0) ? 0 : (0xFFFFFFFFu << (32 - prefix));
			if (a->FirstGatewayAddress) {
				auto* g = reinterpret_cast<sockaddr_in*>(a->FirstGatewayAddress->Address.lpSockaddr);
				info.gateway = ntohl(g->sin_addr.s_addr);
			}
			int n = 0;
			for (auto* d = a->FirstDnsServerAddress; d && n < 2; d = d->Next) {
				auto* dsa = reinterpret_cast<sockaddr_in*>(d->Address.lpSockaddr);
				uint32_t dns = ntohl(dsa->sin_addr.s_addr);
				if (n == 0) {
					info.dns1 = dns;
				} else {
					info.dns2 = dns;
				}
				++n;
			}
			return true;
		}
	}
	return false;
}
#else
bool sock_get_net_info(SockNetInfo& info)
{
	struct ifaddrs* ifa = nullptr;
	if (getifaddrs(&ifa) != 0) return false;
	bool found = false;
	for (auto* it = ifa; it; it = it->ifa_next) {
		if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
		if (it->ifa_flags & IFF_LOOPBACK) continue;
		if (!(it->ifa_flags & IFF_UP)) continue;
		auto* sa = reinterpret_cast<const sockaddr_in*>(it->ifa_addr);
		uint32_t ip = ntohl(sa->sin_addr.s_addr);
		if (ip == 0) continue;
		info.ip = ip;
		if (it->ifa_netmask) {
			auto* sm = reinterpret_cast<const sockaddr_in*>(it->ifa_netmask);
			info.netmask = ntohl(sm->sin_addr.s_addr);
		}
		found = true;
		break;
	}
	freeifaddrs(ifa);
	if (!found) return false;

#ifndef __APPLE__
	// Default gateway (Linux): /proc/net/route, first default (00000000)
	// route. The address is printed as a little-endian hex value.
	std::ifstream route("/proc/net/route");
	std::string line;
	while (std::getline(route, line)) {
		if (line.compare(0, 4, "Iface") == 0) continue; // header
		std::istringstream iss(line);
		std::string iface, dest, gw, flags;
		iss >> iface >> dest >> gw >> flags;
		if (dest != "00000000") continue; // not the default route
		unsigned long f = std::strtoul(flags.c_str(), nullptr, 16);
		if (!(f & 0x1)) continue; // RTF_UP
		info.gateway = ntohl(std::strtoul(gw.c_str(), nullptr, 16));
		break;
	}
#endif

	readResolvConfDns(info.dns1, info.dns2);
	return true;
}
#endif

} // namespace openmsx

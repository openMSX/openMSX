#ifndef SOCKET_HH
#define SOCKET_HH

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#ifndef _WIN32
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace openmsx {

#ifndef _WIN32
inline constexpr int OPENMSX_INVALID_SOCKET = -1;
inline constexpr int SOCKET_ERROR = -1;
using SOCKET = int;
#else
// INVALID_SOCKET is #defined as  (SOCKET)(~0)
// but that gives a old-style-cast warning
static const SOCKET OPENMSX_INVALID_SOCKET = static_cast<SOCKET>(~0);
using in_addr_t =  UINT32;
// Winsock APIs take socket-address lengths as int; on POSIX the system
// socklen_t must be used (redefining it here would break recvfrom() and
// friends, which take a socklen_t*).
using socklen_t = int;
#endif

[[nodiscard]] std::string sock_error();
void sock_close(SOCKET sd);
[[nodiscard]] ptrdiff_t sock_recv(SOCKET sd, char* buf, size_t count);
[[nodiscard]] ptrdiff_t sock_send(SOCKET sd, const char* buf, size_t count);
[[nodiscard]] ptrdiff_t sock_sendto(SOCKET sd, const char* buf, size_t count, const struct sockaddr* to, socklen_t tolen);
[[nodiscard]] ptrdiff_t sock_recvfrom(SOCKET sd, char* buf, size_t count, struct sockaddr* from, socklen_t* fromlen);

// Host network configuration (IPv4 addresses in network byte order, 0 = unknown)
struct SockNetInfo
{
	uint32_t ip = 0;
	uint32_t netmask = 0;
	uint32_t gateway = 0;
	uint32_t dns1 = 0;
	uint32_t dns2 = 0;
};

/** Detects the host's primary IPv4 network configuration.
  * Returns false if no usable IPv4 address was found.
  */
[[nodiscard]] bool sock_get_net_info(SockNetInfo& info);

////

// Activate the socket subsystem (required on Windows)
void sock_startup(); // should only be called via SockActivator
void sock_cleanup();

struct SocketActivator
{
	SocketActivator(const SocketActivator&) = delete;
	SocketActivator(SocketActivator&&) = delete;
	SocketActivator& operator=(const SocketActivator&) = delete;
	SocketActivator& operator=(SocketActivator&&) = delete;

	SocketActivator()
	{
		if (counter == 0) {
			sock_startup();
		}
		++counter;
	}

	~SocketActivator()
	{
		assert(counter > 0);
		--counter;
		if (counter == 0) {
			sock_cleanup();
		}
	}

private:
	static inline int counter = 0;
};

} // namespace openmsx

#endif

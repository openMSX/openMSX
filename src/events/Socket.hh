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
using socklen_t = int;
#else
// INVALID_SOCKET is #defined as  (SOCKET)(~0)
// but that gives a old-style-cast warning
static const SOCKET OPENMSX_INVALID_SOCKET = static_cast<SOCKET>(~0);
using in_addr_t =  UINT32;
#endif

[[nodiscard]] std::string sock_error();
void sock_close(SOCKET sd);
[[nodiscard]] ptrdiff_t sock_recv(SOCKET sd, char* buf, size_t count);
[[nodiscard]] ptrdiff_t sock_send(SOCKET sd, const char* buf, size_t count);

// Make socket 'sd' non-blocking.
void sock_setNonBlocking(SOCKET sd);
// Set an integer/boolean socket option (wraps the Windows 'const char*' cast).
void sock_setIntOption(SOCKET sd, int level, int optName, int value = 1);
// Non-blocking readiness poll (zero timeout): data ready or pending connection.
[[nodiscard]] bool sock_readable(SOCKET sd);
// Build an IPv4 sockaddr_in (network order); hostIp==0 -> INADDR_ANY.
[[nodiscard]] sockaddr_in sock_makeIPv4(uint32_t hostIp, uint16_t port);
// Best-effort local IPv4 (host order, 0 if unknown). Sends no packets.
[[nodiscard]] uint32_t sock_localIPv4();

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

#ifndef SOCKET_HH
#define SOCKET_HH

#include <cstddef>
#include <string>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
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
void sock_startup();
void sock_cleanup();
void sock_close(SOCKET sd);
[[nodiscard]] ptrdiff_t sock_recv(SOCKET sd, char* buf, size_t count);
[[nodiscard]] ptrdiff_t sock_send(SOCKET sd, const char* buf, size_t count);

} // namespace openmsx

#endif

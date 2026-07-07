#include "Socket.hh"

#include "MSXException.hh" // FatalError
#include "utf8_checked.hh"

#include <bit>
#include <cerrno>
#include <cstring>

namespace openmsx {

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

void sock_setNonBlocking(SOCKET sd)
{
#ifdef _WIN32
	u_long mode = 1;
	ioctlsocket(sd, FIONBIO, &mode);
#else
	int flags = fcntl(sd, F_GETFL, 0);
	fcntl(sd, F_SETFL, flags | O_NONBLOCK);
#endif
}

void sock_setIntOption(SOCKET sd, int level, int optName, int value)
{
	setsockopt(sd, level, optName, std::bit_cast<const char*>(&value), sizeof(value));
}

bool sock_readable(SOCKET sd)
{
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(sd, &rfds);
	timeval tv = {0, 0};
	return select(static_cast<int>(sd) + 1, &rfds, nullptr, nullptr, &tv) > 0;
}

sockaddr_in sock_makeIPv4(uint32_t hostIp, uint16_t port)
{
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(hostIp);
	addr.sin_port = htons(port);
	return addr;
}

uint32_t sock_localIPv4()
{
	SOCKET sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd == OPENMSX_INVALID_SOCKET) return 0;
	sockaddr_in remote = sock_makeIPv4(0x08080808, 53);
	uint32_t ip = 0;
	if (connect(sd, std::bit_cast<sockaddr*>(&remote), sizeof(remote)) == 0) {
		sockaddr_in local = {};
		::socklen_t len = sizeof(local);
		if (getsockname(sd, std::bit_cast<sockaddr*>(&local), &len) == 0) {
			ip = ntohl(local.sin_addr.s_addr);
		}
	}
	sock_close(sd);
	return ip;
}

} // namespace openmsx

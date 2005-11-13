// $Id$

#include "Socket.hh"
#include <map>

namespace openmsx {

std::string sock_error()
{
#ifdef _WIN32
	typedef std::map<int, std::string> ErrMap;
	static std::string UnknownError = "Unknown error";
	static ErrMap errMap;
	static bool alreadyInit = false;
	if (!alreadyInit) {
		alreadyInit = true;
		errMap[0]                  = "No error";
		errMap[WSAEINTR]           = "Interrupted system call";
		errMap[WSAEBADF]           = "Bad file number";
		errMap[WSAEACCES]          = "Permission denied";
		errMap[WSAEFAULT]          = "Bad address";
		errMap[WSAEINVAL]          = "Invalid argument";
		errMap[WSAEMFILE]          = "Too many open sockets";
		errMap[WSAEWOULDBLOCK]     = "Operation would block";
		errMap[WSAEINPROGRESS]     = "Operation now in progress";
		errMap[WSAEALREADY]        = "Operation already in progress";
		errMap[WSAENOTSOCK]        = "Socket operation on non-socket";
		errMap[WSAEDESTADDRREQ]    = "Destination address required";
		errMap[WSAEMSGSIZE]        = "Message too long";
		errMap[WSAEPROTOTYPE]      = "Protocol wrong type for socket";
		errMap[WSAENOPROTOOPT]     = "Bad protocol option";
		errMap[WSAEPROTONOSUPPORT] = "Protocol not supported";
		errMap[WSAESOCKTNOSUPPORT] = "Socket type not supported";
		errMap[WSAEOPNOTSUPP]      = "Operation not supported on socket";
		errMap[WSAEPFNOSUPPORT]    = "Protocol family not supported";
		errMap[WSAEAFNOSUPPORT]    = "Address family not supported";
		errMap[WSAEADDRINUSE]      = "Address already in use";
		errMap[WSAEADDRNOTAVAIL]   = "Can't assign requested address";
		errMap[WSAENETDOWN]        = "Network is down";
		errMap[WSAENETUNREACH]     = "Network is unreachable";
		errMap[WSAENETRESET]       = "Net connection reset";
		errMap[WSAECONNABORTED]    = "Software caused connection abort";
		errMap[WSAECONNRESET]      = "Connection reset by peer";
		errMap[WSAENOBUFS]         = "No buffer space available";
		errMap[WSAEISCONN]         = "Socket is already connected";
		errMap[WSAENOTCONN]        = "Socket is not connected";
		errMap[WSAESHUTDOWN]       = "Can't send after socket shutdown";
		errMap[WSAETOOMANYREFS]    = "Too many references, can't splice";
		errMap[WSAETIMEDOUT]       = "Connection timed out";
		errMap[WSAECONNREFUSED]    = "Connection refused";
		errMap[WSAELOOP]           = "Too many levels of symbolic links";
		errMap[WSAENAMETOOLONG]    = "File name too long";
		errMap[WSAEHOSTDOWN]       = "Host is down";
		errMap[WSAEHOSTUNREACH]    = "No route to host";
		errMap[WSAENOTEMPTY]       = "Directory not empty";
		errMap[WSAEPROCLIM]        = "Too many processes";
		errMap[WSAEUSERS]          = "Too many users";
		errMap[WSAEDQUOT]          = "Disc quota exceeded";
		errMap[WSAESTALE]          = "Stale NFS file handle";
		errMap[WSAEREMOTE]         = "Too many levels of remote in path";
		errMap[WSASYSNOTREADY]     = "Network system is unavailable";
		errMap[WSAVERNOTSUPPORTED] = "Winsock version out of range";
		errMap[WSANOTINITIALISED]  = "WSAStartup not yet called";
		errMap[WSAEDISCON]         = "Graceful shutdown in progress";
		errMap[WSAHOST_NOT_FOUND]  = "Host not found";
		errMap[WSANO_DATA]         = "No host data of that type was found";
	}
	ErrMap::const_iterator it = errMap.find(WSAGetLastError());
	if (it != errMap.end()) {
		return it->second;
	} else {
		return UnknownError;
	}
#else
	return strerror(errno);
#endif
}

void sock_startup()
{
#ifdef _WIN32
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
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

void sock_reuseAddr(int sd)
{
#ifdef _WIN32
	// nothing similar exists in win32?
#else
	// Used so we can re-bind to our port while a previous connection is
	// still in TIME_WAIT state.
	int reuse_addr = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
	           sizeof(reuse_addr));
#endif
}

// close a connection
void sock_close(int sd)
{
#ifdef _WIN32
	closesocket(sd);
#else
	close(sd);
#endif
}


int sock_recv(int sd, char* buf, size_t count)
{
	int num = recv(sd, buf, count, 0);
	if (num >  0) return num; // normal case
	if (num == 0) return -1;  // socket was closed by client
#ifdef _WIN32
	// Something bad happened on the socket.  It could just be a
	// "would block" notification, or it could be something more
	// serious.  WSAEWOULDBLOCK can happen after select() says a
	// socket is readable under Win9x, it doesn't happen on
	// WinNT/2000 or on Unix.
	int err;
	int errlen = sizeof(err);
	getsockopt(sd, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
	if (err == WSAEWOULDBLOCK) return 0;
	return -1;
#else
	if (errno == EWOULDBLOCK) return 0;
	return -1;
#endif
}


int sock_send(int sd, const char* buf, size_t count)
{
	int num = send(sd, buf, count, 0);
	if (num >= 0) return num; // normal case
#ifdef _WIN32
	int err;
	int errlen = sizeof(err);
	getsockopt(sd, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
	if (err == WSAEWOULDBLOCK) return 0;
	return -1;
#else
	if (errno == EWOULDBLOCK) return 0;
	return -1;
#endif
}

} // namespace openmsx

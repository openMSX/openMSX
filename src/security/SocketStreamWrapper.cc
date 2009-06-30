// $Id$

#ifdef _WIN32

#include "SocketStreamWrapper.hh"

namespace openmsx {

SocketStreamWrapper::SocketStreamWrapper(SOCKET userSock)
	: sock(userSock)
{
}

uint32 SocketStreamWrapper::Read(void* buffer, uint32 cb)
{
	int recvd = recv(sock, static_cast<char*>(buffer), cb, 0);
	if (recvd == 0 || recvd == SOCKET_ERROR) {
		return STREAM_ERROR;
	}
	return recvd;
}

uint32 SocketStreamWrapper::Write(void* buffer, uint32 cb)
{
	int sent = send(sock, static_cast<char*>(buffer), cb, 0);
	if (sent == 0 || sent == SOCKET_ERROR) {
		return STREAM_ERROR;
	}
	return sent;
}

} // namespace openmsx

#endif

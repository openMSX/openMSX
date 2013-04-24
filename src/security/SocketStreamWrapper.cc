#ifdef _WIN32

#include "SocketStreamWrapper.hh"

namespace openmsx {

using namespace sspiutils;

SocketStreamWrapper::SocketStreamWrapper(SOCKET userSock)
	: sock(userSock)
{
}

uint32_t SocketStreamWrapper::Read(void* buffer, uint32_t cb)
{
	int recvd = recv(sock, static_cast<char*>(buffer), cb, 0);
	if (recvd == 0 || recvd == SOCKET_ERROR) {
		return STREAM_ERROR;
	}
	return recvd;
}

uint32_t SocketStreamWrapper::Write(void* buffer, uint32_t cb)
{
	int sent = send(sock, static_cast<char*>(buffer), cb, 0);
	if (sent == 0 || sent == SOCKET_ERROR) {
		return STREAM_ERROR;
	}
	return sent;
}

} // namespace openmsx

#endif

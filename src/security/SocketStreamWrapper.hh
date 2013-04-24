#ifndef SOCKET_STREAM_WRAPPER_HH
#define SOCKET_STREAM_WRAPPER_HH

#ifdef _WIN32

#include <winsock2.h>
#include "SspiUtils.hh"

namespace openmsx {

class SocketStreamWrapper : public sspiutils::StreamWrapper
{
public:
	explicit SocketStreamWrapper(SOCKET userSock);

	uint32_t Read (void* buffer, uint32_t cb);
	uint32_t Write(void* buffer, uint32_t cb);

private:
	SOCKET sock;
};

} // namespace openmsx

#endif // _WIN32

#endif // SOCKET_STREAM_WRAPPER_HH

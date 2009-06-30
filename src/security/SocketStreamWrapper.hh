// $Id$

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

	uint32 Read (void* buffer, uint32 cb);
	uint32 Write(void* buffer, uint32 cb);

private:
	SOCKET sock;
};

} // namespace openmsx

#endif // _WIN32

#endif // SOCKET_STREAM_WRAPPER_HH

// $Id$

#ifndef SOCKET_STREAM_WRAPPER_HH
#define SOCKET_STREAM_WRAPPER_HH

#ifdef _WIN32

#include <winsock2.h>
#include "SspiUtils.hh"

namespace openmsx {

class SocketStreamWrapper : public StreamWrapper
{
private:
	SOCKET sock;

public:
	SocketStreamWrapper(SOCKET userSock);

	unsigned Read(void* buffer, unsigned cb);
	unsigned Write(void* buffer, unsigned cb);
};


} // namespace openmsx

#endif // _WIN32

#endif // SOCKET_STREAM_WRAPPER_HH

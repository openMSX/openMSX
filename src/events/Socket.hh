// $Id$

#ifndef SOCKET_HH
#define SOCKET_HH

#include <errno.h>
#include <string>

#ifndef __WIN32__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <winsock.h>
#endif

namespace openmsx {

#ifndef __WIN32__
static const int INVALID_SOCKET = -1;
static const int SOCKET_ERROR = -1;
typedef int SOCKET;
#endif

std::string sock_error();
void sock_startup();
void sock_cleanup();
void sock_reuseAddr(int sd);
void sock_close(int sd);
int sock_recv(int sd, char* buf, size_t count);
int sock_send(int sd, const char* buf, size_t count);

} // namespace openmsx

#endif

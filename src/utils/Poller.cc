#include "Poller.hh"

#ifndef _WIN32
#include <cstdio>
#include <poll.h>
#include <unistd.h>
#endif


namespace openmsx {

Poller::Poller()
	: abortFlag(false)
{
#ifndef _WIN32
	if (pipe(wakeupPipe.data())) {
		wakeupPipe[0] = wakeupPipe[1] = -1;
		perror("Failed to open wakeup pipe");
	}
#endif
}

Poller::~Poller()
{
#ifndef _WIN32
	close(wakeupPipe[0]);
	close(wakeupPipe[1]);
#endif
}

void Poller::abort()
{
	abortFlag = true;
#ifndef _WIN32
	char dummy = 'X';
	if (write(wakeupPipe[1], &dummy, sizeof(dummy)) == -1) {
		// Nothing we can do here; we'll have to rely on the poll() timeout.
	}
#endif
}

#ifndef _WIN32
bool Poller::poll(int fd)
{
	while (true) {
		std::array fds = {
			pollfd{.fd = fd,            .events = POLLIN, .revents = 0},
			pollfd{.fd = wakeupPipe[0], .events = POLLIN, .revents = 0},
		};
		int pollResult = ::poll(fds.data(), fds.size(), 1000);
		if (abortFlag) {
			return true;
		}
		if (pollResult == -1) { // error
			return true;
		}
		if (pollResult != 0) { // no timeout
			return false;
		}
	}
}
#endif

} // namespace openmsx

// $Id$

#include <cstddef>
#include <cassert>
#include <SDL_thread.h>
#include "Thread.hh"

namespace openmsx {

Thread::Thread(Runnable* runnable_)
	: runnable(runnable_)
{
	thread = NULL;
}

Thread::~Thread()
{
	stop();
}

void Thread::start()
{
	assert(!thread);
	thread = SDL_CreateThread(startThread, runnable);
}

void Thread::stop()
{
	if (thread) {
		SDL_KillThread(thread);
		thread = NULL;
	}
}

// TODO: A version with timeout would be useful.
//       After the timeout expires, the method would return false.
//       Alternatively, stop() is called if the thread does not end
//       within the given timeout.
void Thread::join()
{
	assert(thread);
	SDL_WaitThread(thread, NULL);
	thread = NULL;
}

int Thread::startThread(void* runnable)
{
	static_cast<Runnable*>(runnable)->run();
	return 0;
}

} // namespace openmsx

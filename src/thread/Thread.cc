// $Id$

#include <SDL/SDL_thread.h>
#include <cstddef>
#include <cassert>
#include "Thread.hh"


Thread::Thread(Runnable *runnable_)
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
	assert(thread == NULL);
	thread = SDL_CreateThread(startThread, runnable);
}

void Thread::stop()
{
	if (thread != NULL) {
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
	assert(thread != NULL);
	SDL_WaitThread(thread, NULL);
	thread = NULL;
}

int Thread::startThread(void *runnable)
{
	((Runnable *)runnable)->run();
	return 0;
}

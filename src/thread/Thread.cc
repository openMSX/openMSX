// $Id$

#include <SDL/SDL_thread.h>
#include <cstddef>
#include "Thread.hh"


Thread::Thread(Runnable *runnable)
{
	thread = NULL;
	this->runnable = runnable;
}

Thread::~Thread()
{
	stop();
}

void Thread::start()
{
	if (thread == NULL)
		thread = SDL_CreateThread(startThread, runnable);
}

void Thread::stop()
{
	if (thread != NULL) {
		SDL_KillThread(thread);
		thread = NULL;
	}
}

int Thread::startThread(void *runnable)
{
	((Runnable *)runnable)->run();
	return 0;
}

// $Id$

#include "Thread.hh"


Thread::Thread(Runnable *runnable)
{
	this->runnable = runnable;
}

void Thread::start()
{
	thread = SDL_CreateThread(startThread, runnable);
}

void Thread::stop()
{
	SDL_KillThread(thread);
}

int Thread::startThread(void *runnable)
{
	((Runnable *)runnable)->run();
	return 0;
}

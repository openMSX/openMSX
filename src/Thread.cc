// $Id$

#include "Thread.hh"

Thread::Thread(Runnable* _runnable)
{
	runnable = _runnable;
}

void Thread::start()
{
	thread = SDL_CreateThread(startThread, runnable);
}

void Thread::stop()
{
	SDL_KillThread(thread);
}

int Thread::startThread(Runnable* runnable)
{
	runnable->run();
	return 0;
}

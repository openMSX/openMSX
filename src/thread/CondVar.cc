// $Id: $

#include "CondVar.hh"
#include <SDL.h>

namespace openmsx {

CondVar::CondVar()
{
	mutex = SDL_CreateMutex();
	cond  = SDL_CreateCond();
}

CondVar::~CondVar()
{
	SDL_DestroyCond(cond);
	SDL_DestroyMutex(mutex);
}

void CondVar::wait()
{
	SDL_mutexP(mutex);
	SDL_CondWait(cond, mutex);
}

bool CondVar::waitTimeout(unsigned us)
{
	SDL_mutexP(mutex);
	int result = SDL_CondWaitTimeout(cond, mutex, us / 1000);
	return result == SDL_MUTEX_TIMEDOUT;
}

void CondVar::signal()
{
	SDL_CondSignal(cond);
}

void CondVar::signalAll()
{
	SDL_CondBroadcast(cond);
}

} // namespace openmsx

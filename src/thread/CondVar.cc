// $Id$

#include <SDL.h>
#include "CondVar.hh"

namespace openmsx {

CondVar::CondVar()
{
	mutex = SDL_CreateMutex();
	cond  = SDL_CreateCond();
}

CondVar::~CondVar()
{
	SDL_DestroyMutex(mutex);
	SDL_DestroyCond(cond);
}

void CondVar::wait()
{
	SDL_mutexP(mutex);
	SDL_CondWait(cond, mutex);
}

int CondVar::waitTimeout(Uint32 to)
{
	SDL_mutexP(mutex);
	return	SDL_CondWaitTimeout(cond, mutex, to);
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

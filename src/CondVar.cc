// $Id$

#include "CondVar.hh"


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

void CondVar::signal()
{
	SDL_CondSignal(cond);
}

void CondVar::signalAll()
{
	SDL_CondBroadcast(cond);
}

// $Id$

#include "Mutex.hh"

Mutex outputmutex, errormutex;

Mutex::Mutex()
{
	mutex = SDL_CreateMutex();
}

Mutex::~Mutex()
{
	SDL_DestroyMutex(mutex);
}

void Mutex::grab()
{
	SDL_mutexP(mutex);
}

void Mutex::release()
{
	SDL_mutexV(mutex);
}

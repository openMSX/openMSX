// $Id$

#include <SDL.h>
#include "Mutex.hh"

namespace openmsx {

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

} // namespace openmsx

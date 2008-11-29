// $Id$

#include "Semaphore.hh"
#include "openmsx.hh"

namespace openmsx {

Semaphore::Semaphore(unsigned value)
{
	semaphore = SDL_CreateSemaphore(value);
}

Semaphore::~Semaphore()
{
	PRT_DEBUG("Destroying semaphore thread: " << SDL_ThreadID() << "   lock: " << this);
	SDL_DestroySemaphore(semaphore);
	PRT_DEBUG("DONE destroying semaphore thread: " << SDL_ThreadID() << "   lock: " << this);
}

void Semaphore::up()
{
	while (SDL_SemPost(semaphore)) {
		// the SDL doc lists no reason why this call could fail,
		// but just in case we try till it succeeds
		PRT_DEBUG("Semaphore up of thread: " << SDL_ThreadID() << " and lock: " << this << " failed, error: " << SDL_GetError());
	}
}

void Semaphore::down()
{
	while (SDL_SemWait(semaphore)) {
		// SDL_SemWait gets interrupted when this thread gets a signal, for
		// example when another thread exits.
		// We don't want to leave before we actually acquired the semaphore,
		// so try again until we have it.
		PRT_DEBUG("Semaphore down of thread: " << SDL_ThreadID() << " and lock: " << this << " failed, error: " << SDL_GetError());
	}
}

} // namespace openmsx

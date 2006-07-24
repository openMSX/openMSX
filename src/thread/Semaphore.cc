// $Id$

#include "Semaphore.hh"

namespace openmsx {

Semaphore::Semaphore(unsigned value)
{
	semaphore = SDL_CreateSemaphore(value);
}

Semaphore::~Semaphore()
{
	SDL_DestroySemaphore(semaphore);
}

void Semaphore::up()
{
	while (SDL_SemPost(semaphore)) {
		// the SDL doc lists no reason why this call could fail,
		// but just in case we try till it succeeds
	}
}

void Semaphore::down()
{
	while (SDL_SemWait(semaphore)) {
		// SDL_SemWait gets interrupted when this thread gets a signal, for
		// example when another thread exits.
		// We don't want to leave before we actually acquired the semaphore,
		// so try again until we have it.
	}
}

} // namespace openmsx

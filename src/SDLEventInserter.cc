#include "SDLEventInserter.hh"

SDLEventInserter::SDLEventInserter(SDL_Event &evnt, const Emutime &time)
{
	event = evnt;
	Scheduler::instance()->setSyncPoint(time, *this);
}

SDLEventInserter::~SDLEventInserter()
{
}

void SDLEventInserter::executeUntilEmuTime(const Emutime &time)
{
	SDL_PushEvent(&event);
	delete this;	// job is done
}


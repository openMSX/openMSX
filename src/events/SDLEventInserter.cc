// $Id$

#include "SDLEventInserter.hh"
#include "Scheduler.hh"


namespace openmsx {

SDLEventInserter::SDLEventInserter(SDL_Event &evnt, const EmuTime &time)
{
	event = evnt;
	Scheduler::instance()->setSyncPoint(time, this);
}

SDLEventInserter::~SDLEventInserter()
{
}

void SDLEventInserter::executeUntilEmuTime(const EmuTime &time, int userData)
{
	SDL_PushEvent(&event);
	delete this;	// job is done
}

} // namespace openmsx


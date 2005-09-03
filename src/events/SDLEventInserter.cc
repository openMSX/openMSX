// $Id$

#include "SDLEventInserter.hh"

using std::string;

namespace openmsx {

SDLEventInserter::SDLEventInserter(Scheduler& scheduler, SDL_Event& evnt,
                                   const EmuTime& time)
	: Schedulable(scheduler)
{
	event = evnt;
	setSyncPoint(time);
}

SDLEventInserter::~SDLEventInserter()
{
}

void SDLEventInserter::executeUntil(const EmuTime& /*time*/, int /*userData*/)
{
	SDL_PushEvent(&event);
	delete this;	// job is done
}

const string& SDLEventInserter::schedName() const
{
	static const string name("SDLEventInserter");
	return name;
}

} // namespace openmsx


// $Id$

#ifndef SDLEVENTINSERTER_HH
#define SDLEVENTINSERTER_HH

#include <SDL.h>
#include "Schedulable.hh"

namespace openmsx {

class SDLEventInserter : private Schedulable
{
public:
	SDLEventInserter(Scheduler& scheduler, SDL_Event& event,
	                 const EmuTime& time);
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

protected:
	virtual ~SDLEventInserter();

private:
	SDL_Event event;
};

} // namespace openmsx

#endif

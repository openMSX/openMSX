// $Id$

#ifndef __SDLEVENTINSERTER_HH__
#define __SDLEVENTINSERTER_HH__

#include <SDL/SDL.h>
#include "Schedulable.hh"


namespace openmsx {

class SDLEventInserter : private Schedulable
{
public:
	SDLEventInserter(SDL_Event &event, const EmuTime &time);
	virtual void executeUntil(const EmuTime &time, int userData) throw();
	
protected:
	virtual ~SDLEventInserter();

private:
	SDL_Event event;
};

} // namespace openmsx

#endif

// $Id$

#ifndef __SDLEVENTINSERTER_HH__
#define __SDLEVENTINSERTER_HH__

#include <SDL/SDL.h>
#include "Schedulable.hh"


class SDLEventInserter : public Schedulable
{
	public:
		SDLEventInserter(SDL_Event &event, const EmuTime &time);
		virtual void executeUntilEmuTime(const EmuTime &time, int userData);
		
	protected:
		virtual ~SDLEventInserter();

	private:
		SDL_Event event;
};

#endif

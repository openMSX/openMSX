// $Id$

#ifndef __SDLEVENTINSERTER_HH__
#define __SDLEVENTINSERTER_HH__

#include <SDL/SDL.h>
#include "Scheduler.hh"

// forward declarations
class EmuTime;


class SDLEventInserter : public Schedulable
{
	public:
		SDLEventInserter(SDL_Event &event, const EmuTime &time);
		void executeUntilEmuTime(const EmuTime &time, int userData);
		
	protected:
		virtual ~SDLEventInserter();

	private:
		SDL_Event event;
};

#endif

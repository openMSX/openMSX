#ifndef __SDLEVENTINSERTER_HH__
#define __SDLEVENTINSERTER_HH__

#include "EmuTime.hh"
#include "Scheduler.hh"
#include <SDL/SDL.h>

class SDLEventInserter : public Schedulable
{
	public:
		SDLEventInserter(SDL_Event &event, const EmuTime &time);
		void executeUntilEmuTime(const EmuTime &time);
		
	protected:
		virtual ~SDLEventInserter();

	private:
		SDL_Event event;
};

#endif

#ifndef __SDLEVENTINSERTER_HH__
#define __SDLEVENTINSERTER_HH__

#include "emutime.hh"
#include "Scheduler.hh"
#include <SDL/SDL.h>

class SDLEventInserter : public Schedulable
{
	public:
		SDLEventInserter(SDL_Event &event, const Emutime &time);
		void executeUntilEmuTime(const Emutime &time);
		
	protected:
		virtual ~SDLEventInserter();

	private:
		SDL_Event event;
};

#endif

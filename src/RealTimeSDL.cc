// $Id$

#include "RealTimeSDL.hh"
#include <SDL/SDL.h>

namespace openmsx {

RealTimeSDL::RealTimeSDL()
{
	initBase();
}

unsigned RealTimeSDL::getRealTime()
{
	return SDL_GetTicks() * 1000; // ms -> us
}

void RealTimeSDL::doSleep(unsigned us)
{
	SDL_Delay(us / 1000);
}

void RealTimeSDL::reset()
{
}

} // namespace openmsx


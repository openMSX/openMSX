// $Id$

#include "RealTimeSDL.hh"
#include <SDL/SDL.h>

namespace openmsx {

RealTimeSDL::RealTimeSDL()
{
	initBase();
}

unsigned RealTimeSDL::getTime()
{
	return SDL_GetTicks();
}

void RealTimeSDL::doSleep(unsigned ms)
{
	SDL_Delay(ms);
}

void RealTimeSDL::reset()
{
}

} // namespace openmsx


// $Id$

#include "Alarm.hh"
#include <SDL.h>

namespace openmsx {

Alarm::Alarm(unsigned interval)
{
	id = SDL_AddTimer(interval / 1000, helper, this);
}

Alarm::~Alarm()
{
	if (id) SDL_RemoveTimer(static_cast<SDL_TimerID>(id));
}

unsigned Alarm::helper(unsigned interval, void* param)
{
	Alarm* alarm = static_cast<Alarm*>(param);
	alarm->alarm();
	alarm->id = 0;
	return 0;
}

} // namespace openmsx

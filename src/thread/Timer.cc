// $Id$

#include <SDL.h>
#include "Timer.hh"

#include "probed_defs.hh"
#ifdef HAVE_GETTIMEOFDAY
#include <sys/time.h>
#include <time.h>
#endif
#ifdef HAVE_USLEEP
#include <unistd.h>
#endif


// TODO add us precision timer routines for win32

namespace openmsx {

unsigned long long Timer::getTime()
{
#ifdef HAVE_GETTIMEOFDAY
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned long long)tv.tv_sec * 1000000 +
	       (unsigned long long)tv.tv_usec;
#else
	return (unsigned long long)SDL_GetTicks() * 1000;
#endif
}

void Timer::sleep(unsigned long long us)
{
#ifdef HAVE_USLEEP
	usleep(us);
#else
	SDL_Delay(us / 1000);
#endif
}

} // namespace openmsx


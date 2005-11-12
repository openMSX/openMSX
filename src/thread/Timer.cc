// $Id$

#include "Timer.hh"
#include "probed_defs.hh"
#ifdef HAVE_GETTIMEOFDAY
#include <sys/time.h>
#include <time.h>
#endif
#ifdef HAVE_USLEEP
#include <unistd.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#include <SDL.h>

namespace openmsx {

namespace Timer {

static inline unsigned long long getSDLTicks()
{
	return (unsigned long long)SDL_GetTicks() * 1000;
}

unsigned long long getTime()
{
#if defined (WIN32)
	static LONGLONG hfFrequency = 0;

	LARGE_INTEGER li;
	if (!hfFrequency) {
		if (QueryPerformanceFrequency(&li)) {
			hfFrequency = li.QuadPart;
		} else {
			return getSDLTicks();
		}
	}
	QueryPerformanceCounter(&li);

	// Assumes that the timer never wraps. The mask is just to
	// ensure that the multiplication doesn't wrap.
	return (li.QuadPart & ((long long)-1 >> 20)) * 1000000 / hfFrequency;

#elif defined (HAVE_GETTIMEOFDAY)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned long long)tv.tv_sec * 1000000 +
	       (unsigned long long)tv.tv_usec;
#else
	return getSDLTicks();
#endif
}

#ifdef WIN32
static void CALLBACK timerCallback(unsigned int,
                                   unsigned int,
                                   unsigned long eventHandle,
                                   unsigned long,
                                   unsigned long)
{
    SetEvent((HANDLE)eventHandle);
}
#endif

void sleep(unsigned long long us)
{
#if defined (WIN32)
	us /= 1000;
	if (us > 0) {
		static HANDLE timerEvent = NULL;
		if (timerEvent == NULL) {
			timerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		}
		UINT id = timeSetEvent(us, 1, timerCallback, (DWORD)timerEvent,
		                       TIME_ONESHOT);
		WaitForSingleObject(timerEvent, INFINITE);
		timeKillEvent(id);
	}
#elif defined (HAVE_USLEEP)
	usleep(us);
#else
	SDL_Delay(us / 1000);
#endif
}

} // namespace Timer

} // namespace openmsx

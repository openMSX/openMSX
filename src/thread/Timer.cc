#include "Timer.hh"
#include "systemfuncs.hh"
#if HAVE_USLEEP
#include <unistdp.hh>
#endif
#include <chrono>
#include <SDL.h>

namespace openmsx {
namespace Timer {

// non-static to avoid unused function warning
inline uint64_t getSDLTicks()
{
	return static_cast<uint64_t>(SDL_GetTicks()) * 1000;
}

uint64_t getTime()
{
	static uint64_t lastTime = 0;
	uint64_t now;

#ifndef _MSC_VER
	using namespace std::chrono;
	now = duration_cast<microseconds>(
		steady_clock::now().time_since_epoch()).count();
#else
	// Visual studio 2012 does offer std::chrono, but unfortunately it's
	// buggy and low resolution. So for now we still use SDL. See also:
	//   http://stackoverflow.com/questions/11488075/vs11-is-steady-clock-steady
	//   https://connect.microsoft.com/VisualStudio/feedback/details/753115/
	now = static_cast<uint64_t>(SDL_GetTicks()) * 1000;
#endif

	// Other parts of openMSX may crash if this function ever returns a
	// value that is less than a previously returned value. Hence this
	// extra check.
	// SDL_GetTicks() is not guaranteed to return monotonic values.
	// steady_clock OTOH should be monotonic. It's implemented in terms of
	// clock_gettime(CLOCK_MONOTONIC). Unfortunately in older linux
	// versions we've seen buggy implementation that once in a while did
	// return time points slightly in the past.
	if (now < lastTime) return lastTime;
	lastTime = now;
	return now;
}

/*#if defined _WIN32
static void CALLBACK timerCallback(unsigned int,
                                   unsigned int,
                                   unsigned long eventHandle,
                                   unsigned long,
                                   unsigned long)
{
    SetEvent((HANDLE)eventHandle);
}
#endif*/

void sleep(uint64_t us)
{
/*#if defined _WIN32
	us /= 1000;
	if (us > 0) {
		static HANDLE timerEvent = nullptr;
		if (!timerEvent) {
			timerEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		}
		UINT id = timeSetEvent(us, 1, timerCallback, (DWORD)timerEvent,
		                       TIME_ONESHOT);
		WaitForSingleObject(timerEvent, INFINITE);
		timeKillEvent(id);
	}
*/
#if HAVE_USLEEP
	usleep(us);
#else
	SDL_Delay(unsigned(us / 1000));
#endif
}

} // namespace Timer
} // namespace openmsx

#include "Timer.hh"
#include "systemfuncs.hh"
#if HAVE_CLOCK_GETTIME
#include <ctime>
#endif
#if HAVE_USLEEP
#include <unistdp.hh>
#endif
#if defined _WIN32
#include <windows.h>
#endif
#include <SDL.h>
#include <cassert>

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
/* QueryPerformanceCounter() has problems on modern CPUs,
 *  - on dual core CPUs time can ge backwards (a bit) when your process
 *    get scheduled on the other core
 *  - the resolution of the timer can vary on CPUs that can change its
 *    clock frequency (for power managment)
##if defined _WIN32
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
	now = (li.QuadPart & (int64_t(-1) >> 20)) * 1000000 / hfFrequency;
*/
	// clock_gettime doesn't seem to work properly on MinGW/Win32 cross compilation
#if HAVE_CLOCK_GETTIME && defined(_POSIX_MONOTONIC_CLOCK) && !(defined(_WIN32) && defined(__GNUC__))
	// Note: in the past we used the more portable gettimeofday() function,
	//       but the result of that function is not always monotonic.
	timespec ts;
	int result = clock_gettime(CLOCK_MONOTONIC, &ts);
	assert(result == 0); (void)result;
	now = static_cast<uint64_t>(ts.tv_sec) * 1000000 +
	      static_cast<uint64_t>(ts.tv_nsec) / 1000;
#else
	now = getSDLTicks();
#endif
	if (now < lastTime) {
		// This shouldn't happen, time should never go backwards.
		// Though there appears to be a bug in some Linux kernels
		// so that occasionally clock_gettime(CLOCK_MONOTONIC) _does_
		// go back in time slightly. When that happens we return the
		// last time again.
		return lastTime;
	}
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

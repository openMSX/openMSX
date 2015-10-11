#include "Timer.hh"
#include "systemfuncs.hh"
#if HAVE_USLEEP
#include <unistdp.hh>
#endif
#include <chrono>
#include <SDL.h>

namespace openmsx {
namespace Timer {

uint64_t getTime()
{
	static uint64_t lastTime = 0;
	uint64_t now;

	using namespace std::chrono;
	now = duration_cast<microseconds>(
		steady_clock::now().time_since_epoch()).count();

	// Other parts of openMSX may crash if this function ever returns a
	// value that is less than a previously returned value. Hence this
	// extra check.
	// Steady_clock _should_ be monotonic. It's implemented in terms of
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

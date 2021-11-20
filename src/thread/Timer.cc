#include "Timer.hh"
#include <chrono>
#include <thread>

namespace openmsx::Timer {

uint64_t getTime()
{
	static uint64_t lastTime = 0;

	using namespace std::chrono;
	uint64_t now = duration_cast<microseconds>(
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

void sleep(uint64_t us)
{
	std::this_thread::sleep_for(std::chrono::microseconds(us));
}

} // namespace openmsx::Timer

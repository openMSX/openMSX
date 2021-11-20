#ifndef TIMER_HH
#define TIMER_HH

#include <cstdint>

namespace openmsx::Timer {

	/** Get current (real) time in us. Absolute value has no meaning.
	  */
	[[nodiscard]] uint64_t getTime();

	/** Sleep for the specified amount of time (in us). It is possible
	  * that this method sleeps longer or shorter than the requested time.
	  */
	void sleep(uint64_t us);

} // namespace openmsx::Timer

#endif

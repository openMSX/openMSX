// $Id$

#ifndef CPUCLOCK_HH
#define CPUCLOCK_HH

#include "DynamicClock.hh"
#include "likely.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

class CPUClock
{
protected:
	explicit CPUClock(const EmuTime& time);

// benchmarking showed a slowdown of ~3% on AMD64
// when using the following code:
#if 0
	// 64-bit addition is cheap
	inline void add(unsigned ticks) { clock += ticks; }
	inline void sync() const { }
#else
	// 64-bit addition is expensive
	// (if executed several million times per second)
	inline void add(unsigned ticks) { extra += ticks; }
	inline void sync() const {
		if (limit >= 0) limit -= extra;
		clock.fastAdd(extra); extra = 0;
	}
#endif

	// These are similar to the corresponding methods in DynamicClock.
	const EmuTime& getTime() const { sync(); return clock.getTime(); }
	void setTime(const EmuTime& time) { sync(); clock.reset(time); }
	void advanceTime(const EmuTime& time) { sync(); clock.advance(time); }
	void setFreq(unsigned freq) { clock.setFreq(freq); }

	/** Implementation of the HALT instruction timing.
	  * Advances the clock with an integer multiple of 'hltStates' cycles
	  * so that it equal or bigger than 'time'. Returns the number of times
	  * 'hltStates' needed to be added.
	  */
	unsigned advanceHalt(unsigned hltStates, const EmuTime& time) {
		sync();
		unsigned ticks = clock.getTicksTillUp(time);
		unsigned halts = (ticks + hltStates - 1) / hltStates; // round up
		clock += halts * hltStates;
		return halts;
	}

	// The following 3 methods are used in the innermost CPU loop. It
	// allows to implement this loop with just one test. This loop must
	// be exited on the following three conditions:
	//   1) a Synchronization Point is reached
	//   2) a 'slow' instruction must be executed (ei, di, ..)
	//   3) another thread has requested to exit the loop
	// The limitReached() method indicates whether the loop should be
	// exited.
	// The earliest SP must always be kept up-to-date with the setLimit()
	// method, so after every action that might change SP. Luckily that
	// doesn't happen in too many places (only IO and uncached memory
	// access).
	// In 'slow' mode the limit mechanism can be disabled with the
	// enableLimit() method. In disabled mode, the limitReached() method
	// always returns true.
	// When another thread requests to exit the loop, it's not needed to
	// already exit at the next instruction. If we exit soon that's good
	// enough. This is implemented by simply regularly exiting the loop
	// (outside the inner loop, the real exit condition should be tested).

	void setLimit(const EmuTime& time) {
		if (likely(limitEnabled)) {
			if (unlikely(limitTime != time)) {
				setLimit_slow(time);
			}
		} else {
			assert(limit < 0);
		}
	}
	void enableLimit(bool enable_) {
		limitEnabled = enable_;
		if (limitEnabled) {
			limitTime = EmuTime::infinity;
		} else {
			limit = -1;
		}
	}
	bool limitReached() const {
		return limit <= extra;
	}

private:
	void setLimit_slow(const EmuTime& time);

	mutable DynamicClock clock;
	EmuTime limitTime;
	mutable int extra;
	mutable int limit;
	bool limitEnabled;
};

} // namespace openmsx

#endif

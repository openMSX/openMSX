#ifndef CPUCLOCK_HH
#define CPUCLOCK_HH

#include "DynamicClock.hh"
#include "Scheduler.hh"
#include "narrow.hh"
#include <cassert>

namespace openmsx {

class CPUClock
{
public:
	unsigned getFreq() const { return clock.getFreq(); }

protected:
	CPUClock(EmuTime::param time, Scheduler& scheduler);

// benchmarking showed a slowdown of ~3% on AMD64
// when using the following code:
#if 0
	// 64-bit addition is cheap
	void add(unsigned ticks) { clock += ticks; }
	void sync() const { }
#else
	// 64-bit addition is expensive
	// (if executed several million times per second)
	void add(unsigned ticks) { remaining -= narrow_cast<int>(ticks); }
	void sync() const {
		clock.fastAdd(limit - remaining);
		limit = remaining;
	}
#endif

	// These are similar to the corresponding methods in DynamicClock.
	[[nodiscard]] EmuTime::param getTime() const { sync(); return clock.getTime(); }
	[[nodiscard]] EmuTime getTimeFast() const { return clock.getFastAdd(limit - remaining); }
	[[nodiscard]] EmuTime getTimeFast(int cc) const {
		return clock.getFastAdd(limit - remaining + cc);
	}
	void setTime(EmuTime::param time) { sync(); clock.reset(time); }
	void setFreq(unsigned freq) { sync(); disableLimit(); clock.setFreq(freq); }
	void advanceTime(EmuTime::param time);
	[[nodiscard]] EmuTime calcTime(EmuTime::param time, unsigned ticks) const {
		return clock.add(time, ticks);
	}

	/** Implementation of the HALT instruction timing.
	  * Advances the clock with an integer multiple of 'hltStates' cycles
	  * so that it equal or bigger than 'time'. Returns the number of times
	  * 'hltStates' needed to be added.
	  */
	unsigned advanceHalt(unsigned hltStates, EmuTime::param time) {
		sync();
		unsigned ticks = clock.getTicksTillUp(time);
		unsigned halts = (ticks + hltStates - 1) / hltStates; // round up
		clock += halts * hltStates;
		return halts;
	}

	/** R800 runs at 7MHz, but I/O is done over a slower 3.5MHz bus. So
	  * sometimes right before I/O it's needed to wait for one cycle so
	  * that we're at the start of a clock cycle of the slower bus.
	  */
	void waitForEvenCycle(int cc)
	{
		sync();
		auto totalTicks = clock.getTotalTicks() + cc;
		if (totalTicks & 1) {
			add(1);
		}
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
	// method, so after every action that might change SP. The Scheduler
	// class is responsible for this.
	// In 'slow' mode the limit mechanism can be disabled with the
	// disableLimit() method. In disabled mode, the limitReached() method
	// always returns true.
	// When another thread requests to exit the loop, it's not needed to
	// already exit at the next instruction. If we exit soon that's good
	// enough. This is implemented by simply regularly exiting the loop
	// (outside the inner loop, the real exit condition should be tested).

	void setLimit(EmuTime::param time) {
		if (limitEnabled) {
			sync();
			assert(remaining == limit);
			limit = narrow_cast<int>(clock.getTicksTillUp(time) - 1);
			remaining = limit;
		} else {
			assert(limit < 0);
		}
	}
	void enableLimit() {
		limitEnabled = true;
		setLimit(scheduler.getNext());
	}
	void disableLimit() {
		limitEnabled = false;
		int extra = limit - remaining;
		limit = -1;
		remaining = limit - extra;
	}
	[[nodiscard]] bool limitReached() const {
		return remaining < 0;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	mutable DynamicClock clock;
	Scheduler& scheduler;
	int remaining = -1;
	mutable int limit = -1;
	bool limitEnabled = false;
};

} // namespace openmsx

#endif

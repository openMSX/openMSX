// $Id$

#ifndef CPUCLOCK_HH
#define CPUCLOCK_HH

#include "DynamicClock.hh"

namespace openmsx {

class CPUClock
{
protected:
	CPUClock(const EmuTime& time)
		: clock(time), extra(0)
	{
	}

// TODO benchmark this on AMD64
#if 0 
	// 64-bit addition is cheap
	inline void add(unsigned ticks) { clock += ticks; }
	inline void sync() const { }
#else
	// 64-bit addition is expensive
	// (if executed several million times per second)
	inline void add(unsigned ticks) { extra += ticks; }
	inline void sync() const { clock += extra; extra = 0; }
#endif

	const EmuTime& getTime() const { sync(); return clock.getTime(); }
	void setTime(const EmuTime& time) { clock.reset(time); }
	void advanceTime(const EmuTime& time) { sync(); clock.advance(time); }
	void setFreq(unsigned freq) { clock.setFreq(freq); }
	unsigned advanceHalt(unsigned hltStates, const EmuTime& time) {
		sync();
		uint64 ticks = clock.getTicksTillUp(time);
		unsigned halts = (ticks + hltStates - 1) / hltStates; // round up
		clock += halts * hltStates;
		return halts;
	}

private:
	mutable DynamicClock clock;
	mutable unsigned extra;
};

} // namespace openmsx

#endif

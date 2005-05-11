// $Id$

#ifndef CLOCK_HH
#define CLOCK_HH

#include "EmuDuration.hh"
#include "EmuTime.hh"
#include <cassert>

namespace openmsx {

/** Represents a clock with a fixed frequency.
  * The frequency is in Hertz, so every tick is 1/frequency second.
  * A clock has a current time, which can be increased by
  * an integer number of ticks.
  */
template <unsigned freq>
class Clock
{
public:
	/** Calculates the duration of the given number of ticks at this
	  * clock's frequency.
	  */
	static const EmuDuration duration(unsigned ticks) {
		return EmuDuration(ticks * (MAIN_FREQ / freq));
	}

	/** Create a new clock, which starts ticking at time zero.
	  */
	Clock() : lastTick() { }

	/** Create a new clock, which starts ticking at the given time.
	  */
	explicit Clock(const EmuTime& e)
		: lastTick(e) { }

	/** Gets the time at which the last clock tick occurred.
	  */
	const EmuTime& getTime() const { return lastTick; }

	/** Checks whether this clock's last tick is or is not before the
	  * given time stamp.
	  */
	bool before(const EmuTime& e) const {
		return lastTick.time < e.time;
	}

	/** Calculate the number of ticks for this clock until the given time.
	  * It is not allowed to call this method for a time in the past.
	  */
	unsigned getTicksTill(const EmuTime& e) const {
		assert(e.time >= lastTick.time);
		return (e.time - lastTick.time) / (MAIN_FREQ / freq);
	}

	/** Calculate the time at which this clock will have ticked the given
	  * number of times (counted from its last tick).
	  */
	const EmuTime operator+(uint64 n) const {
		return EmuTime(lastTick.time + n * (MAIN_FREQ / freq));
	}

	/** Reset the clock to start ticking at the given time.
	  */
	void reset(const EmuTime& e) {
		lastTick.time = e.time;
	}

	/** Advance this clock in time until the last tick which is not past
	  * the given time.
	  * It is not allowed to advance a clock to a time in the past.
	  */
	void advance(const EmuTime& e) {
		assert(lastTick.time <= e.time);
		lastTick.time = e.time - (e.time - lastTick.time) % (MAIN_FREQ / freq);
	}

	/** Advance this clock by the given number of ticks.
	  */
	void operator+=(unsigned n) {
		lastTick.time += n * (MAIN_FREQ / freq);
	}

private:
	/** Time of this clock's last tick.
	  */
	EmuTime lastTick;
};

} // namespace openmsx

#endif

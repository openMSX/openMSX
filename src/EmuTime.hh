// $Id$

#ifndef __EMUTIME_HH__
#define __EMUTIME_HH__

#include <iostream>
#include <cassert>
#include "openmsx.hh"

using std::ostream;

namespace openmsx {

// constants
const uint64 MAIN_FREQ = 3579545ULL * 1200ULL;


class EmuDuration
{
public:
	// friends
	friend class EmuTime;

	// constructors
	EmuDuration()                  : time(0) {}
	explicit EmuDuration(uint64 n) : time(n) {}
	explicit EmuDuration(double duration)
		: time((uint64)(duration * MAIN_FREQ)) {}

	// conversions
	double toDouble() const { return (double)time / MAIN_FREQ; }
	uint64 length() const { return time; }
	unsigned frequency() const { return MAIN_FREQ / time; }

	// assignment operator
	EmuDuration& operator=(const EmuDuration& d)
		{ time = d.time; return *this; }

	// comparison operators
	bool operator==(const EmuDuration& d) const
		{ return time == d.time; }
	bool operator!=(const EmuDuration& d) const
		{ return time != d.time; }
	bool operator< (const EmuDuration& d) const
		{ return time <  d.time; }
	bool operator<=(const EmuDuration& d) const
		{ return time <= d.time; }
	bool operator> (const EmuDuration& d) const
		{ return time >  d.time; }
	bool operator>=(const EmuDuration& d) const
		{ return time >= d.time; }

	// arithmetic operators
	const EmuDuration operator%(const EmuDuration& d) const
		{ return EmuDuration(time % d.time); }
	const EmuDuration operator*(unsigned fact) const
		{ return EmuDuration(time * fact); }
	unsigned operator/(const EmuDuration& d) const
		{ return time / d.time; }
	
	EmuDuration& operator*=(double fact)
		{ time = static_cast<uint64>(time * fact); return *this; }
	EmuDuration& operator/=(double fact)
		{ time = static_cast<uint64>(time / fact); return *this; }

	// ticks
	// TODO: Used in WavAudioInput. Keep or use DynamicClock instead?
	unsigned getTicksAt(unsigned freq) const
		{ return time / (MAIN_FREQ / freq); }

	static const EmuDuration zero;
	static const EmuDuration infinity;

private:
	uint64 time;
};

class EmuTime
{
public:
	// friends
	friend ostream& operator<<(ostream& os, const EmuTime& time);
	template<unsigned> friend class Clock;
	friend class DynamicClock;

	// constructors
	EmuTime()                  : time(0) {}
	explicit EmuTime(uint64 n) : time(n) {}
	EmuTime(const EmuTime& e)  : time(e.time) {}

	// destructor
	virtual ~EmuTime();

	// assignment operator
	EmuTime& operator=(const EmuTime& e)
		{ time = e.time; return *this; }

	// comparison operators
	bool operator==(const EmuTime& e) const
		{ return time == e.time; }
	bool operator!=(const EmuTime& e) const
		{ return time != e.time; }
	bool operator< (const EmuTime& e) const
		{ return time <  e.time; }
	bool operator<=(const EmuTime& e) const
		{ return time <= e.time; }
	bool operator> (const EmuTime& e) const
		{ return time >  e.time; }
	bool operator>=(const EmuTime& e) const
		{ return time >= e.time; }

	// arithmetic operators
	const EmuTime operator+(const EmuDuration& d) const
		{ return EmuTime(time + d.time); }
	const EmuTime operator-(const EmuDuration& d) const
		{ assert(time >= d.time);
		  return EmuTime(time - d.time); }
	EmuTime& operator+=(const EmuDuration& d)
		{ time += d.time; return *this; }
	EmuTime& operator-=(const EmuDuration& d)
		{ assert(time >= d.time);
		  time -= d.time; return *this; }
	const EmuDuration operator-(const EmuTime& e) const
		{ assert(time >= e.time);
		  return EmuDuration(time - e.time); }

	static const EmuTime zero;
	static const EmuTime infinity;

private:
	uint64 time;
};

ostream& operator <<(ostream& os, const EmuTime& e);

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
		: lastTick(e.time - e.time % (MAIN_FREQ / freq)) { }

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

/** Represents a clock with a variable frequency.
  * The frequency is in Hertz, so every tick is 1/frequency second.
  * A clock has a current time, which can be increased by
  * an integer number of ticks.
  */
class DynamicClock
{
public:
	/** Create a new clock, which starts ticking at time zero.
	  * The initial frequency is infinite;
	  * in other words, the clock stands still.
	  */
	DynamicClock() : lastTick(), step(0) { }

	/** Gets the time at which the last clock tick occurred.
	  */
	const EmuTime& getTime() const {
		return lastTick;
	}

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
		return (e.time - lastTick.time) / step;
	}

	/** Calculate the number of ticks this clock has to tick to reach
	  * or go past the given time.
	  * It is not allowed to call this method for a time in the past.
	  * TODO: This method is only used for implementing the HALT instruction.
	  *       Maybe it's possible to calculate this in another way?
	  */
	unsigned getTicksTillUp(const EmuTime& e) const { 
		assert(e.time >= lastTick.time);
		return (e.time - lastTick.time + step - 1) / step; // round up
	}

	/** Change the frequency at which this clock ticks.
	  * @param freq New frequency in Hertz.
	  */
	void setFreq(unsigned freq) {
		assert((MAIN_FREQ / freq) < (1ull << 32));
		step = MAIN_FREQ / freq;
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
		lastTick.time = e.time - (e.time - lastTick.time) % step;
	}

	/** Advance this clock by the given number of ticks.
	  */
	void operator+=(unsigned n) {
		#ifdef DEBUG 
		// we don't even want this overhead in development versions
		assert(((uint64)n * step) < (1ull << 32));
		#endif
		lastTick.time += n * step;
	}

private:
	/** Time of this clock's last tick.
	  */
	EmuTime lastTick;

	/** Length of a this clock's ticks, expressed in master clock ticks.
	  */
	unsigned step; // changed uint64 -> unsigned for performance reasons
	               // this is _heavily_ used in the CPU code
};

} // namespace openmsx

#endif

#ifndef DYNAMICCLOCK_HH
#define DYNAMICCLOCK_HH

#include "EmuTime.hh"
#include "DivModBySame.hh"
#include <cassert>

namespace openmsx {

/** Represents a clock with a variable frequency.
  * The frequency is in Hertz, so every tick is 1/frequency second.
  * A clock has a current time, which can be increased by
  * an integer number of ticks.
  */
class DynamicClock
{
public:
	// Note: default copy constructor and assigment operator are ok.

	/** Create a new clock, which starts ticking at given time.
	  * The initial frequency is infinite;
	  * in other words, the clock stands still.
	  */
	explicit DynamicClock(EmuTime::param time) : lastTick(time) {}

	/** Create a new clock, which starts ticking at given time with
	  * given frequency.
	  */
	DynamicClock(EmuTime::param time, unsigned freq)
		: lastTick(time)
	{
		setFreq(freq);
	}

	/** Gets the time at which the last clock tick occurred.
	  */
	EmuTime::param getTime() const {
		return lastTick;
	}

	/** Checks whether this clock's last tick is or is not before the
	  * given time stamp.
	  */
	bool before(EmuTime::param e) const {
		return lastTick.time < e.time;
	}

	/** Calculate the number of ticks for this clock until the given time.
	  * It is not allowed to call this method for a time in the past.
	  */
	unsigned getTicksTill(EmuTime::param e) const {
		assert(e.time >= lastTick.time);
		return divmod.div(e.time - lastTick.time);
	}

	template<typename FIXED>
	void getTicksTill(EmuTime::param e, FIXED& result) const {
		assert(e.time >= lastTick.time);
		uint64_t tmp = (e.time - lastTick.time) << FIXED::FRACTION_BITS;
		result = FIXED::create(divmod.div(tmp + (getStep() / 2)));
	}

	/** Calculate the number of ticks this clock has to tick to reach
	  * or go past the given time.
	  * It is not allowed to call this method for a time in the past.
	  */
	unsigned getTicksTillUp(EmuTime::param e) const {
		assert(e.time >= lastTick.time);
		return divmod.div(e.time - lastTick.time + (getStep() - 1));
	}

	double getTicksTillDouble(EmuTime::param e) const {
		assert(e.time >= lastTick.time);
		return double(e.time - lastTick.time) / getStep();
	}

	uint64_t getTotalTicks() const {
		// note: don't use divmod.div() because that one only returns a
		//       32 bit result. Maybe improve in the future.
		return lastTick.time / getStep();
	}

	/** Change the frequency at which this clock ticks.
	  * @param freq New frequency in Hertz.
	  */
	void setFreq(unsigned freq) {
		unsigned newStep = (MAIN_FREQ32 + (freq / 2)) / freq;
		assert(newStep);
		divmod.setDivisor(newStep);
	}
	/** Equivalent to setFreq(freq_num / freq_denom), but possibly with
	  * less rounding errors.
	  */
	void setFreq(unsigned freq_num, unsigned freq_denom) {
		static_assert(MAIN_FREQ < (1ull << 32), "must fit in 32 bit");
		uint64_t p = MAIN_FREQ * freq_denom + (freq_num / 2);
		uint64_t newStep = p / freq_num;
		assert(newStep < (1ull << 32));
		assert(newStep);
		divmod.setDivisor(unsigned(newStep));
	}

	/** Returns the frequency (in Hz) at which this clock ticks.
	  * @see setFreq()
	  */
	unsigned getFreq() const {
		return MAIN_FREQ32 / getStep();
	}

	/** Returns the length of one clock-cycle.
	  */
	EmuDuration getPeriod() const {
		return EmuDuration(uint64_t(getStep()));
	}

	/** Reset the clock to start ticking at the given time.
	  */
	void reset(EmuTime::param e) {
		lastTick.time = e.time;
	}

	/** Advance this clock in time until the last tick which is not past
	  * the given time.
	  * It is not allowed to advance a clock to a time in the past.
	  */
	void advance(EmuTime::param e) {
		assert(lastTick.time <= e.time);
		lastTick.time = e.time - divmod.mod(e.time - lastTick.time);
	}

	/** Advance this clock by the given number of ticks.
	  */
	void operator+=(uint64_t n) {
		lastTick.time += n * getStep();
	}

	/** Calculate the time at which this clock will have ticked the given
	  * number of times (counted from its last tick).
	  */
	const EmuTime operator+(uint64_t n) const {
		return EmuTime(lastTick.time + n * getStep());
	}

	/** Advance this clock by the given number of ticks.
	  * This method is similar to operator+=, but it's optimized for
	  * speed. OTOH the amount of ticks should not be too large,
	  * otherwise an overflow occurs. Use operator+() when the duration
	  * of the ticks approaches 1 second.
	  */
	void fastAdd(unsigned n) {
		#ifdef DEBUG
		// we don't even want this overhead in development versions
		assert((uint64_t(n) * getStep()) < (1ull << 32));
		#endif
		lastTick.time += n * getStep();
	}
	EmuTime getFastAdd(unsigned n) const {
		return add(lastTick, n);
	}
	EmuTime add(EmuTime::param time, unsigned n) const {
		#ifdef DEBUG
		assert((uint64_t(n) * getStep()) < (1ull << 32));
		#endif
		return EmuTime(time.time + n * getStep());
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/** Length of a this clock's ticks, expressed in master clock ticks.
	  *   changed uint64_t -> unsigned for performance reasons
	  *   this is _heavily_ used in the CPU code
	  * We used to store this as a member, but DivModBySame also stores
	  * it, so we can as well get it from there (getter is inlined).
	  */
	unsigned getStep() const { return divmod.getDivisor(); }

	/** Time of this clock's last tick.
	  */
	EmuTime lastTick;

	DivModBySame divmod;
};

} // namespace openmsx

#endif

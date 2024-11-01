#ifndef CLOCK_HH
#define CLOCK_HH

#include "EmuDuration.hh"
#include "EmuTime.hh"
#include "DivModByConst.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

/** Represents a clock with a fixed frequency.
  * The frequency is in Hertz, so every tick is 1/frequency second.
  * A clock has a current time, which can be increased by
  * an integer number of ticks.
  */
template<unsigned FREQ_NUM, unsigned FREQ_DENOM = 1>
class Clock
{
private:
	// stuff below calculates:
	//   MASTER_TICKS = MAIN_FREQ / (FREQ_NUM / FREQ_DENOM) + 0.5
	static_assert(MAIN_FREQ < (1ULL << 32), "must fit in 32 bit");
	static constexpr uint64_t P = MAIN_FREQ * FREQ_DENOM + (FREQ_NUM / 2);
	static constexpr uint64_t MASTER_TICKS = P / FREQ_NUM;
	static_assert(MASTER_TICKS < (1ULL << 32), "must fit in 32 bit");
	static constexpr unsigned MASTER_TICKS32 = MASTER_TICKS;

public:
	// Note: default copy constructor and assignment operator are ok.

	/** Calculates the duration of the given number of ticks at this
	  * clock's frequency.
	  */
	[[nodiscard]] static constexpr EmuDuration duration(unsigned ticks) {
		return EmuDuration(ticks * MASTER_TICKS);
	}

	/** Create a new clock, which starts ticking at the given time.
	  */
	constexpr explicit Clock(EmuTime::param e)
		: lastTick(e) { }

	/** Gets the time at which the last clock tick occurred.
	  */
	[[nodiscard]] constexpr EmuTime::param getTime() const { return lastTick; }

	/** Checks whether this clock's last tick is or is not before the
	  * given time stamp.
	  */
	[[nodiscard]] constexpr bool before(EmuTime::param e) const {
		return lastTick.time < e.time;
	}

	/** Calculate the number of ticks for this clock until the given time.
	  * It is not allowed to call this method for a time in the past.
	  */
	[[nodiscard]] constexpr unsigned getTicksTill(EmuTime::param e) const {
		assert(e.time >= lastTick.time);
		uint64_t result = (e.time - lastTick.time) / MASTER_TICKS;
#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
#endif
		return unsigned(result);
	}
	/** Same as above, only faster, Though the time interval may not
	  * be too large.
	  */
	[[nodiscard]] constexpr unsigned getTicksTill_fast(EmuTime::param e) const {
		assert(e.time >= lastTick.time);
		DivModByConst<MASTER_TICKS32> dm;
		return dm.div(e.time - lastTick.time);
	}
	/** Calculate the number of ticks this clock has to tick to reach
	  * or go past the given time.
	  * It is not allowed to call this method for a time in the past.
	  */
	[[nodiscard]] constexpr uint64_t getTicksTillUp(EmuTime::param e) const {
		assert(e.time >= lastTick.time);
		return (e.time - lastTick.time + MASTER_TICKS - 1) / MASTER_TICKS;
	}

	/** Calculate the time at which this clock will have ticked the given
	  * number of times (counted from its last tick).
	  */
	// TODO should be friend, workaround for pre-gcc-13 bug
	[[nodiscard]] constexpr EmuTime operator+(uint64_t n) const {
		return EmuTime(lastTick.time + n * MASTER_TICKS);
	}

	/** Like operator+() but faster, though the step can't be too big (max
	  * a little over 1 second). */
	[[nodiscard]] constexpr EmuTime getFastAdd(unsigned n) const {
		#ifdef DEBUG
		assert((uint64_t(n) * MASTER_TICKS) < (1ULL << 32));
		#endif
		return EmuTime(lastTick.time + n * MASTER_TICKS);
	}

	/** Reset the clock to start ticking at the given time.
	  */
	constexpr void reset(EmuTime::param e) {
		lastTick.time = e.time;
	}

	/** Advance this clock in time until the last tick which is not past
	  * the given time.
	  * It is not allowed to advance a clock to a time in the past.
	  */
	constexpr void advance(EmuTime::param e) {
		assert(lastTick.time <= e.time);
		lastTick.time = e.time - ((e.time - lastTick.time) % MASTER_TICKS);
	}
	/** Same as above, only faster, Though the time interval may not
	  * be too large.
	  */
	constexpr void advance_fast(EmuTime::param e) {
		assert(lastTick.time <= e.time);
		DivModByConst<MASTER_TICKS32> dm;
		lastTick.time = e.time - dm.mod(e.time - lastTick.time);
	}

	/** Advance this clock by the given number of ticks.
	  */
	constexpr void operator+=(unsigned n) {
		lastTick.time += n * MASTER_TICKS;
	}

	/** Advance this clock by the given number of ticks.
	  * This method is similar to operator+=, but it's optimized for
	  * speed. OTOH the amount of ticks should not be too large,
	  * otherwise an overflow occurs. Use operator+() when the duration
	  * of the ticks approaches 1 second.
	  */
	constexpr void fastAdd(unsigned n) {
		#ifdef DEBUG
		// we don't even want this overhead in development versions
		assert((n * MASTER_TICKS) < (1ULL << 32));
		#endif
		lastTick.time += n * MASTER_TICKS32;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("lastTick", lastTick);
	}

private:
	/** Time of this clock's last tick.
	  */
	EmuTime lastTick;
};

template<unsigned FREQ_NUM, unsigned FREQ_DENOM>
struct SerializeAsMemcpy<Clock<FREQ_NUM, FREQ_DENOM>> : std::true_type {};

} // namespace openmsx

#endif

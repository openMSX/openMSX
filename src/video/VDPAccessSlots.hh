#ifndef VDPACCESSSLOTS_HH
#define VDPACCESSSLOTS_HH

#include "VDP.hh"
#include <cassert>
#include <cstdint>

namespace openmsx::VDPAccessSlots {

constexpr int TICKS = VDP::TICKS_PER_LINE;

enum Delta : int {
	DELTA_0    =  0 * TICKS,
	DELTA_1    =  1 * TICKS,
	DELTA_16   =  2 * TICKS,
	DELTA_24   =  3 * TICKS,
	DELTA_28   =  4 * TICKS,
	DELTA_32   =  5 * TICKS,
	DELTA_40   =  6 * TICKS,
	DELTA_48   =  7 * TICKS,
	DELTA_64   =  8 * TICKS,
	DELTA_72   =  9 * TICKS,
	DELTA_88   = 10 * TICKS,
	DELTA_104  = 11 * TICKS,
	DELTA_120  = 12 * TICKS,
	DELTA_128  = 13 * TICKS,
	DELTA_136  = 14 * TICKS,
	NUM_DELTAS = 15,
};

/** VDP-VRAM access slot calculator, meant to be used in the inner loops of the
  * VDPCmdEngine commands. Code optimized for the case that:
  *  - timing remains constant (sprites/display enable/disable)
  *  - there are more calls to next() and limitReached() than to getTime()
  */
class Calculator
{
public:
	/** This shouldn't be called directly, instead use getCalculator(). */
	Calculator(EmuTime::param frame, EmuTime::param time,
	           EmuTime::param limit_, const uint8_t* tab_)
		: ref(frame), tab(tab_)
	{
		assert(frame <= time);
		assert(frame <= limit_);
		// not required that time <= limit

		ticks = ref.getTicksTill_fast(time);
		limit = ref.getTicksTill_fast(limit_);
		int lines = ticks / TICKS;
		ticks -= lines * TICKS;
		limit -= lines * TICKS; // might be negative
		ref   += lines * TICKS;
		assert(0 <= ticks); assert(ticks < TICKS);
	}

	/** Has 'time' advanced to or past 'limit'? */
	[[nodiscard]] inline bool limitReached() const {
		return ticks >= limit;
	}

	/** Get the current time. Initially this will return the 'time'
	  * constructor parameter. Each call to next() will increase this
	  * value. */
	[[nodiscard]] inline EmuTime getTime() const {
		return ref.getFastAdd(ticks);
	}

	/** Advance time to the earliest access slot that is at least 'delta'
	  * ticks later than the current time. */
	inline void next(Delta delta) {
		ticks += tab[delta + ticks];
		if (ticks >= TICKS) [[unlikely]] {
			ticks -= TICKS;
			limit -= TICKS;
			ref   += TICKS;
		}
	}

private:
	int ticks;
	int limit;
	VDP::VDPClock ref;
	const uint8_t* const tab;
};

/** Return the time of the next available access slot that is at least 'delta'
  * cycles later than 'time'. The start of the current 'frame' is needed for
  * reference. */
[[nodiscard]] EmuTime getAccessSlot(EmuTime::param frame, EmuTime::param time, Delta delta,
                      const VDP& vdp);

/** When many calls to getAccessSlot() are needed, it's more efficient to
  * instead use this function. */
[[nodiscard]] Calculator getCalculator(
	EmuTime::param frame, EmuTime::param time, EmuTime::param limit,
	const VDP& vdp);

} // namespace openmsx::VDPAccessSlots

#endif

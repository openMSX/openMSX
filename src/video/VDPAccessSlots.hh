#ifndef VDPACCESSSLOTS_HH
#define VDPACCESSSLOTS_HH

#include "VDP.hh"

#include "narrow.hh"

#include <cassert>
#include <cstdint>
#include <span>
#include <utility>

namespace openmsx::VDPAccessSlots {

inline constexpr int TICKS = VDP::TICKS_PER_LINE;

/** Minimum distance until the next VRAM access. */
enum class Delta : int {
	D0        =  0 * TICKS, // These 2 are internal helpers
	D1        =  1 * TICKS,
	CPU_16    =  2 * TICKS, // These 2 are for CPU access delays (V99x8)
	CPU_28    =  3 * TICKS, //                                   (TMS99x8)
	CMD_24    =  4 * TICKS, // The remaining ones are command engine steps
	CMD_32    =  5 * TICKS, //   counted in 'memory cycles' rather than 'VDP cycles'
	CMD_36    =  6 * TICKS, //   see the comment about 'stretch1' and 'stretch2' in
	CMD_46    =  7 * TICKS, //   VDPAccessSlots.cc
	CMD_60    =  8 * TICKS,
	CMD_72    =  9 * TICKS,
	CMD_84    = 10 * TICKS,
	CMD_88    = 11 * TICKS,
	CMD_36_68 = 12 * TICKS, // 36+68 = 104
	CMD_46_58 = 12 * TICKS, // 46+58 = 104 (notice: duplicate! skip in the FIRST/LAST indices below)
	CMD_84_36 = 13 * TICKS, // 84+36 = 120
	CMD_60_68 = 14 * TICKS, // 60+68 = 128
	CMD_72_58 = 15 * TICKS, // 72+58 = 130
};
static constexpr int NUM_DELTAS = 16;
/** The CPU access delays in the 'Delta' enum, D16 and D28. */
static constexpr int FIRST_CPU_DELTA = 2;
static constexpr int LAST_CPU_DELTA = 4; // exclusive
/** The first command engine step in the 'Delta' enum; everything from here on
  * is subject to the memory-cycle counting. */
static constexpr int FIRST_CMD_DELTA = 4;
static constexpr int LAST_CMD_DELTA = NUM_DELTAS; // exclusive

/** VDP-VRAM access slot calculator, meant to be used in the inner loops of the
  * VDPCmdEngine commands. Code optimized for the case that:
  *  - timing remains constant (sprites/display enable/disable)
  *  - there are more calls to next() and limitReached() than to getTime()
  */
class Calculator
{
public:
	/** This shouldn't be called directly, instead use getCalculator(). */
	Calculator(EmuTime frame, EmuTime time,
	           EmuTime limit_, std::span<const uint8_t, NUM_DELTAS * TICKS> tab_)
		: ref(frame), tab(tab_)
	{
		assert(frame <= time);
		assert(frame <= limit_);
		// not required that time <= limit

		ticks = narrow<int>(ref.getTicksTill_fast(time));
		limit = narrow<int>(ref.getTicksTill_fast(limit_));
		int lines = ticks / TICKS;
		ticks -= lines * TICKS;
		limit -= lines * TICKS; // might be negative
		ref   += lines * TICKS;
		assert(0 <= ticks); assert(ticks < TICKS);
	}

	/** Has 'time' advanced to or past 'limit'? */
	[[nodiscard]] bool limitReached() const {
		return ticks >= limit;
	}

	/** Get the current time. Initially this will return the 'time'
	  * constructor parameter. Each call to next() will increase this
	  * value. */
	[[nodiscard]] EmuTime getTime() const {
		return ref.getFastAdd(ticks);
	}

	/** Advance time to the earliest access slot that is at least 'delta'
	  * ticks later than the current time. */
	void next(Delta delta) {
		ticks += tab[std::to_underlying(delta) + ticks];
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
	std::span<const uint8_t, NUM_DELTAS * TICKS> tab;
};

/** Return the time of the next available access slot that is at least 'delta'
  * cycles later than 'time'. The start of the current 'frame' is needed for
  * reference. */
[[nodiscard]] EmuTime getAccessSlot(EmuTime frame, EmuTime time, Delta delta,
                      const VDP& vdp);

/** When many calls to getAccessSlot() are needed, it's more efficient to
  * instead use this function. */
[[nodiscard]] Calculator getCalculator(
	EmuTime frame, EmuTime time, EmuTime limit,
	const VDP& vdp);

} // namespace openmsx::VDPAccessSlots

#endif

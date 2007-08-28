// $Id:$

#include "CPUClock.hh"

namespace openmsx {

CPUClock::CPUClock(const EmuTime& time)
	: clock(time), limitTime(EmuTime::infinity)
	, extra(0), limit(-1), limitEnabled(false)
{
}

void CPUClock::setLimit_slow(const EmuTime& time)
{
	limitTime = time;
	sync();
	assert(extra == 0);
	int newLimit = std::min(15000u, clock.getTicksTillUp(time));
	if (limit <= 0) {
		limit = newLimit;
	} else {
		limit = std::min(limit, newLimit);
	}
}

} // namespace openmsx

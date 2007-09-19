// $Id:$

#include "CPUClock.hh"
#include "Scheduler.hh"

namespace openmsx {

CPUClock::CPUClock(const EmuTime& time, Scheduler& scheduler_)
	: clock(time)
	, scheduler(scheduler_)
	, extra(0), limit(-1), limitEnabled(false)
{
}

void CPUClock::setLimit(const EmuTime& time)
{
	if (limitEnabled) {
		sync();
		assert(extra == 0);
		int newLimit = std::min(15000u, clock.getTicksTillUp(time));
		if (limit <= 0) {
			limit = newLimit;
		} else {
			limit = std::min(limit, newLimit);
		}
	} else {
		assert(limit < 0);
	}
}

void CPUClock::enableLimit(bool enable_)
{
	limitEnabled = enable_;
	if (limitEnabled) {
		setLimit(scheduler.getNext());
	} else {
		limit = -1;
	}
}

void CPUClock::advanceTime(const EmuTime& time)
{
	extra = 0;
	clock.advance(time);
	setLimit(scheduler.getNext());
}

} // namespace openmsx

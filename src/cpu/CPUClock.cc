// $Id:$

#include "CPUClock.hh"
#include "Scheduler.hh"
#include "serialize.hh"

namespace openmsx {

CPUClock::CPUClock(const EmuTime& time, Scheduler& scheduler_)
	: clock(time)
	, scheduler(scheduler_)
	, remaining(-1), limit(-1), limitEnabled(false)
{
}

void CPUClock::setLimit(const EmuTime& time)
{
	if (limitEnabled) {
		sync();
		assert(remaining == limit);
		int newLimit = std::min(15000u, clock.getTicksTillUp(time));
		if (limit <= 0) {
			limit = newLimit;
		} else {
			limit = std::min(limit, newLimit);
		}
		remaining = limit;
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
		int extra = limit - remaining;
		limit = -1;
		remaining = limit - extra;
	}
}

void CPUClock::advanceTime(const EmuTime& time)
{
	remaining = limit;
	clock.advance(time);
	setLimit(scheduler.getNext());
}

template<typename Archive>
void CPUClock::serialize(Archive& ar, unsigned /*version*/)
{
	sync();
	ar.serialize("clock", clock);
}
INSTANTIATE_SERIALIZE_METHODS(CPUClock);

} // namespace openmsx

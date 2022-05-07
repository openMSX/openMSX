#include "CPUClock.hh"
#include "serialize.hh"

namespace openmsx {

CPUClock::CPUClock(EmuTime::param time, Scheduler& scheduler_)
	: clock(time)
	, scheduler(scheduler_)
	, remaining(-1), limit(-1), limitEnabled(false)
{
}

void CPUClock::advanceTime(EmuTime::param time)
{
	remaining = limit;
	clock.advance(time);
	setLimit(scheduler.getNext());
}

void CPUClock::serialize(Archive auto& ar, unsigned /*version*/)
{
	sync();
	ar.serialize("clock", clock);
}
INSTANTIATE_SERIALIZE_METHODS(CPUClock);

} // namespace openmsx

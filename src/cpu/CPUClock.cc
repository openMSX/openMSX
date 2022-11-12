#include "CPUClock.hh"
#include "serialize.hh"

namespace openmsx {

CPUClock::CPUClock(EmuTime::param time, Scheduler& scheduler_)
	: clock(time)
	, scheduler(scheduler_)
{
}

void CPUClock::advanceTime(EmuTime::param time)
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

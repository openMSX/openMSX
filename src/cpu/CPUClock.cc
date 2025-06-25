#include "CPUClock.hh"
#include "serialize.hh"

namespace openmsx {

CPUClock::CPUClock(EmuTime time, Scheduler& scheduler_)
	: clock(time)
	, scheduler(scheduler_)
{
}

void CPUClock::advanceTime(EmuTime time)
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

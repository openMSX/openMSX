#include "RTSchedulable.hh"

#include "RTScheduler.hh"

namespace openmsx {

RTSchedulable::RTSchedulable(RTScheduler& scheduler_)
	: scheduler(scheduler_)
{
}

RTSchedulable::~RTSchedulable()
{
	cancelRT();
}

void RTSchedulable::scheduleRT(uint64_t delta)
{
	cancelRT();
	scheduler.add(delta, *this);
}

bool RTSchedulable::cancelRT()
{
	return scheduler.remove(*this);
}

bool RTSchedulable::isPendingRT() const
{
	return scheduler.isPending(*this);
}

} // namespace openmsx

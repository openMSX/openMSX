// $Id$

#include "Schedulable.hh"
#include "Scheduler.hh"
#include <iostream>

namespace openmsx {

Schedulable::Schedulable(Scheduler& scheduler_)
	: scheduler(scheduler_)
{
}

Schedulable::~Schedulable()
{
	removeSyncPoints();
}

void Schedulable::schedulerDeleted()
{
	std::cerr << "Internal error: Schedulable \"" << schedName()
	          << "\" failed to unregister." << std::endl;
}

void Schedulable::setSyncPoint(const EmuTime& timestamp, int userData)
{
	scheduler.setSyncPoint(timestamp, *this, userData);
}

void Schedulable::removeSyncPoint(int userData)
{
	scheduler.removeSyncPoint(*this, userData);
}

void Schedulable::removeSyncPoints()
{
	scheduler.removeSyncPoints(*this);
}

bool Schedulable::pendingSyncPoint(int userData)
{
	return scheduler.pendingSyncPoint(*this, userData);
}

Scheduler& Schedulable::getScheduler() const
{
	return scheduler;
}

} // namespace openmsx

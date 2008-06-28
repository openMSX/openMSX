// $Id$

#include "Schedulable.hh"
#include "Scheduler.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
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

const EmuTime& Schedulable::getCurrentTime() const
{
	return scheduler.getCurrentTime();
}

template <typename Archive>
void Schedulable::serialize(Archive& ar, unsigned /*version*/)
{
	if (ar.isLoader()) {
		removeSyncPoints();
		Scheduler::SyncPoints syncPoints;
		ar.serialize("syncPoints", syncPoints);
		for (Scheduler::SyncPoints::const_iterator it = syncPoints.begin();
		     it != syncPoints.end(); ++it) {
			setSyncPoint(it->getTime(), it->getUserData());
		}
	} else {
		Scheduler::SyncPoints syncPoints;
		scheduler.getSyncPoints(syncPoints, *this);
		ar.serialize("syncPoints", syncPoints);
	}
}
INSTANTIATE_SERIALIZE_METHODS(Schedulable);

} // namespace openmsx

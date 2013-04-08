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
	std::cerr << "Internal error: Schedulable \"" << typeid(*this).name()
	          << "\" failed to unregister." << std::endl;
}

void Schedulable::setSyncPoint(EmuTime::param timestamp, int userData)
{
	scheduler.setSyncPoint(timestamp, *this, userData);
}

bool Schedulable::removeSyncPoint(int userData)
{
	return scheduler.removeSyncPoint(*this, userData);
}

void Schedulable::removeSyncPoints()
{
	scheduler.removeSyncPoints(*this);
}

bool Schedulable::pendingSyncPoint(int userData) const
{
	return scheduler.pendingSyncPoint(*this, userData);
}

Scheduler& Schedulable::getScheduler() const
{
	return scheduler;
}

EmuTime::param Schedulable::getCurrentTime() const
{
	return scheduler.getCurrentTime();
}

template <typename Archive>
void Schedulable::serialize(Archive& ar, unsigned /*version*/)
{
	Scheduler::SyncPoints syncPoints;
	if (!ar.isLoader()) {
		syncPoints = scheduler.getSyncPoints(*this);
	}
	ar.serialize("syncPoints", syncPoints);
	if (ar.isLoader()) {
		removeSyncPoints();
		for (auto& s : syncPoints) {
			setSyncPoint(s.getTime(), s.getUserData());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Schedulable);

} // namespace openmsx

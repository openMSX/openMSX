// $Id$

#include "Schedulable.hh"
#include "Scheduler.hh"

namespace openmsx {

Schedulable::Schedulable()
	: scheduler(Scheduler::instance())
{
}

Schedulable::~Schedulable()
{
}

void Schedulable::setSyncPoint(const EmuTime& timestamp, int userData) {
	scheduler.setSyncPoint(timestamp, *this, userData);
}

void Schedulable::removeSyncPoint(int userData) {
	scheduler.removeSyncPoint(*this, userData);
}

bool Schedulable::pendingSyncPoint(int userData) {
	return scheduler.pendingSyncPoint(*this, userData);
}

} // namespace openmsx

// $Id$

#include "Scheduler.hh"
#include "Schedulable.hh"
#include "Thread.hh"
#include <cassert>
#include <algorithm>

namespace openmsx {

Scheduler::Scheduler()
	: scheduleTime(EmuTime::zero)
{
}

Scheduler::~Scheduler()
{
	assert(syncPoints.empty());
}

void Scheduler::setSyncPoint(const EmuTime& time, Schedulable& device, int userData)
{
	//PRT_DEBUG("Sched: registering " << device->schedName() <<
	//          " " << userData << " for emulation at " << time);
	assert(Thread::isMainThread());
	assert(time >= scheduleTime);

	// Push sync point into queue.
	SyncPoints::iterator it =
		std::upper_bound(syncPoints.begin(), syncPoints.end(), time,
		            LessSyncPoint());
	syncPoints.insert(it, SynchronizationPoint(time, &device, userData));
}

void Scheduler::removeSyncPoint(Schedulable& device, int userData)
{
	assert(Thread::isMainThread());
	for (SyncPoints::iterator it = syncPoints.begin();
	     it != syncPoints.end(); ++it) {
		if (((*it).getDevice() == &device) &&
		    ((*it).getUserData() == userData)) {
			syncPoints.erase(it);
			break;
		}
	}
}

void Scheduler::removeSyncPoints(Schedulable& device)
{
	assert(Thread::isMainThread());
	syncPoints.erase(remove_if(syncPoints.begin(), syncPoints.end(),
	                           FindSchedulable(device)),
	                 syncPoints.end());
}

bool Scheduler::pendingSyncPoint(Schedulable& device, int userData)
{
	assert(Thread::isMainThread());
	for (SyncPoints::iterator it = syncPoints.begin();
	     it != syncPoints.end(); ++it) {
		if ((it->getDevice() == &device) &&
		    (it->getUserData() == userData)) {
			return true;
		}
	}
	return false;
}

const EmuTime& Scheduler::getCurrentTime() const
{
	assert(Thread::isMainThread());
	return scheduleTime;
}

void Scheduler::scheduleHelper(const EmuTime& limit)
{
	while (true) {
		// Get next sync point.
		const SynchronizationPoint sp =
			  syncPoints.empty()
			? SynchronizationPoint(EmuTime::infinity, NULL, 0)
			: syncPoints.front();
		const EmuTime& time = sp.getTime();
		if (time > limit) {
			break;
		}

		assert(scheduleTime <= time);
		scheduleTime = time;

		syncPoints.erase(syncPoints.begin());

		Schedulable* device = sp.getDevice();
		assert(device);
		int userData = sp.getUserData();
		//PRT_DEBUG ("Sched: Scheduling " << device->schedName()
		//		<< " " << userData << " till " << time);
		device->executeUntil(time, userData);
	}
}

} // namespace openmsx

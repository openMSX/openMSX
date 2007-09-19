// $Id$

#include "Scheduler.hh"
#include "Schedulable.hh"
#include "Thread.hh"
#include "MSXCPU.hh"
#include <cassert>
#include <algorithm>

namespace openmsx {

Scheduler::Scheduler()
	: scheduleTime(EmuTime::zero)
	, cpu(0)
	, scheduleInProgress(false)
{
}

Scheduler::~Scheduler()
{
	assert(!cpu);
	SyncPoints copy(syncPoints);
	for (SyncPoints::const_iterator it = copy.begin();
	     it != copy.end(); ++it) {
		it->getDevice()->schedulerDeleted();
	}

	assert(syncPoints.empty());
}

void Scheduler::setSyncPoint(const EmuTime& time, Schedulable& device, int userData)
{
	//PRT_DEBUG("Sched: registering " << device.schedName() <<
	//          " " << userData << " for emulation at " << time);
	assert(Thread::isMainThread());
	assert(time >= scheduleTime);

	// Push sync point into queue.
	SyncPoints::iterator it =
		std::upper_bound(syncPoints.begin(), syncPoints.end(), time,
		            LessSyncPoint());
	syncPoints.insert(it, SynchronizationPoint(time, &device, userData));

	if (!scheduleInProgress && cpu) {
		// only when scheduleHelper() is not being executed
		// otherwise getNext() doesn't return the correct time and
		// scheduleHelper() anyway calls setNextSyncPoint() at the end
		cpu->setNextSyncPoint(getNext());
	}
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
	assert(!scheduleInProgress);
	scheduleInProgress = true;
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
		//std::cout << "Sched: Scheduling " << device->schedName()
		//          << " " << userData << " till " << time << std::endl;
		device->executeUntil(time, userData);
	}
	scheduleInProgress = false;

	cpu->setNextSyncPoint(getNext());
}

} // namespace openmsx

// $Id$

#include "Scheduler.hh"
#include "Schedulable.hh"
#include "PollInterface.hh"
#include <cassert>
#include <algorithm>

namespace openmsx {

const EmuTime Scheduler::ASAP(EmuTime::zero);

Scheduler::Scheduler()
	: sem(1), scheduleTime(EmuTime::zero)
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

	ScopedLock lock(sem);
	assert(time == ASAP || time >= scheduleTime);

	// Push sync point into queue.
	SyncPoints::iterator it =
		std::upper_bound(syncPoints.begin(), syncPoints.end(), time,
		            LessSyncPoint());
	syncPoints.insert(it, SynchronizationPoint(time, &device, userData));
}

void Scheduler::removeSyncPoint(Schedulable& device, int userData)
{
	ScopedLock lock(sem);
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
	ScopedLock lock(sem);
	syncPoints.erase(remove_if(syncPoints.begin(), syncPoints.end(),
	                           FindSchedulable(device)),
	                 syncPoints.end());
}

bool Scheduler::pendingSyncPoint(Schedulable& device, int userData)
{
	ScopedLock lock(sem);
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
	return scheduleTime;
}

void Scheduler::scheduleHelper(const EmuTime& limit)
{
	doPoll(); // TODO

	while (true) {
		// Get next sync point.
		sem.down();
		const SynchronizationPoint sp =
			  syncPoints.empty()
			? SynchronizationPoint(EmuTime::infinity, NULL, 0)
			: syncPoints.front();
		const EmuTime& sp_time = sp.getTime();
		if (sp_time > limit) {
			sem.up();
			break;
		}

		EmuTime time((sp_time == ASAP) ? scheduleTime : sp_time);
		assert(scheduleTime <= time);
		scheduleTime = time;

		syncPoints.erase(syncPoints.begin());
		sem.up();

		Schedulable* device = sp.getDevice();
		assert(device);
		int userData = sp.getUserData();
		//PRT_DEBUG ("Sched: Scheduling " << device->schedName()
		//		<< " " << userData << " till " << time);
		device->executeUntil(time, userData);
	}
}

void Scheduler::registerPoll(PollInterface& poll)
{
	assert(std::find(pollInterfaces.begin(), pollInterfaces.end(), &poll) ==
	       pollInterfaces.end());
	pollInterfaces.push_back(&poll);
}

void Scheduler::unregisterPoll(PollInterface& poll)
{
	assert(std::find(pollInterfaces.begin(), pollInterfaces.end(), &poll) !=
	       pollInterfaces.end());
	pollInterfaces.erase(std::find(pollInterfaces.begin(), pollInterfaces.end(), &poll));
}

void Scheduler::doPoll()
{
	for (PollInterfaces::const_iterator it = pollInterfaces.begin();
	     it != pollInterfaces.end(); ++it) {
		(*it)->poll();
	}
}

} // namespace openmsx

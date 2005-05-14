// $Id$

#include "Scheduler.hh"
#include "Schedulable.hh"
#include "InputEventGenerator.hh"
#include "Interpreter.hh"
#include <cassert>
#include <algorithm>

using std::swap;
using std::make_heap;
using std::push_heap;
using std::pop_heap;


namespace openmsx {

const EmuTime Scheduler::ASAP;

Scheduler::Scheduler()
	: sem(1)
{
}

Scheduler::~Scheduler()
{
}

Scheduler& Scheduler::instance()
{
	static Scheduler oneInstance;
	return oneInstance;
}

void Scheduler::setSyncPoint(const EmuTime& time, Schedulable* device, int userData)
{
	//PRT_DEBUG("Sched: registering " << device->schedName() <<
	//          " " << userData << " for emulation at " << time);

	ScopedLock lock(sem);
	assert(device);
	assert(time == ASAP || time >= scheduleTime);

	// Push sync point into queue.
	syncPoints.push_back(SynchronizationPoint(time, device, userData));
	push_heap(syncPoints.begin(), syncPoints.end());
}

void Scheduler::removeSyncPoint(Schedulable* device, int userData)
{
	ScopedLock lock(sem);
	for (SyncPoints::iterator it = syncPoints.begin();
	     it != syncPoints.end(); ++it) {
		SynchronizationPoint& sp = *it;
		if ((sp.getDevice() == device) &&
		    (sp.getUserData() == userData)) {
			swap(sp, syncPoints.back());
			syncPoints.pop_back();
			make_heap(syncPoints.begin(), syncPoints.end());
			return;
		}
	}
}

bool Scheduler::pendingSyncPoint(Schedulable* device, int userData)
{
	ScopedLock lock(sem);
	for (SyncPoints::iterator it = syncPoints.begin();
	     it != syncPoints.end(); ++it) {
		if ((it->getDevice() == device) &&
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
	InputEventGenerator::instance().poll();
	Interpreter::instance().poll();

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

		pop_heap(syncPoints.begin(), syncPoints.end());
		syncPoints.pop_back();
		sem.up();

		Schedulable* device = sp.getDevice();
		assert(device);
		int userData = sp.getUserData();
		//PRT_DEBUG ("Sched: Scheduling " << device->schedName()
		//		<< " " << userData << " till " << time);
		device->executeUntil(time, userData);
	}
	scheduleTime = limit;
}

} // namespace openmsx

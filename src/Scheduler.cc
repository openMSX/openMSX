// $Id$

#include "Scheduler.hh"
#include "Schedulable.hh"
#include "Thread.hh"
#include "MSXCPU.hh"
#include "serialize.hh"
#include <cassert>
#include <algorithm>
#include <iterator> // for back_inserter

namespace openmsx {

struct LessSyncPoint {
	bool operator()(EmuTime::param time,
	                const SynchronizationPoint& sp) const;
	bool operator()(const SynchronizationPoint& sp,
	                EmuTime::param time) const;
	bool operator()(const SynchronizationPoint& lhs,
	                const SynchronizationPoint& rhs) const;
};
bool LessSyncPoint::operator()(
	EmuTime::param time, const SynchronizationPoint& sp) const
{
	return time < sp.getTime();
}
bool LessSyncPoint::operator()(
	const SynchronizationPoint& sp, EmuTime::param time) const
{
	return sp.getTime() < time;
}
bool LessSyncPoint::operator()(
	const SynchronizationPoint& lhs, const SynchronizationPoint& rhs) const
{
	// This method is needed for VC++ debug build (I'm not sure why).
	return lhs.getTime() < rhs.getTime();
}


struct FindSchedulable {
	explicit FindSchedulable(const Schedulable& schedulable);
	bool operator()(const SynchronizationPoint& sp) const;
	const Schedulable& schedulable;
};
FindSchedulable::FindSchedulable(const Schedulable& schedulable_)
	: schedulable(schedulable_)
{
}

bool FindSchedulable::operator()(const SynchronizationPoint& sp) const
{
	return sp.getDevice() == &schedulable;
}


Scheduler::Scheduler()
	: scheduleTime(EmuTime::zero)
	, cpu(nullptr)
	, scheduleInProgress(false)
{
}

Scheduler::~Scheduler()
{
	assert(!cpu);
	auto copy = syncPoints;
	for (auto& s : copy) {
		s.getDevice()->schedulerDeleted();
	}

	assert(syncPoints.empty());
}

void Scheduler::setSyncPoint(EmuTime::param time, Schedulable& device, int userData)
{
	assert(Thread::isMainThread());
	assert(time >= scheduleTime);

	// Push sync point into queue.
	auto it = std::upper_bound(syncPoints.begin(), syncPoints.end(), time,
	                           LessSyncPoint());
	syncPoints.insert(it, SynchronizationPoint(time, &device, userData));

	if (!scheduleInProgress && cpu) {
		// only when scheduleHelper() is not being executed
		// otherwise getNext() doesn't return the correct time and
		// scheduleHelper() anyway calls setNextSyncPoint() at the end
		cpu->setNextSyncPoint(getNext());
	}
}

Scheduler::SyncPoints Scheduler::getSyncPoints(const Schedulable& device) const
{
	SyncPoints result;
	copy_if(syncPoints.begin(), syncPoints.end(), back_inserter(result),
	        FindSchedulable(device));
	return result;
}

bool Scheduler::removeSyncPoint(Schedulable& device, int userData)
{
	assert(Thread::isMainThread());
	for (auto it = syncPoints.begin(); it != syncPoints.end(); ++it) {
		if ((it->getDevice() == &device) &&
		    (it->getUserData() == userData)) {
			syncPoints.erase(it);
			return true;
		}
	}
	return false; // nothing removed
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
	for (auto& s : syncPoints) {
		if ((s.getDevice() == &device) &&
		    (s.getUserData() == userData)) {
			return true;
		}
	}
	return false;
}

EmuTime::param Scheduler::getCurrentTime() const
{
	assert(Thread::isMainThread());
	return scheduleTime;
}

void Scheduler::scheduleHelper(EmuTime::param limit)
{
	assert(!scheduleInProgress);
	scheduleInProgress = true;
	while (true) {
		// Get next sync point.
		const auto& sp = syncPoints.front();
		EmuTime time = sp.getTime();
		if (time > limit) {
			break;
		}

		assert(scheduleTime <= time);
		scheduleTime = time;

		Schedulable* device = sp.getDevice();
		assert(device);
		int userData = sp.getUserData();

		syncPoints.erase(syncPoints.begin());

		device->executeUntil(time, userData);
	}
	scheduleInProgress = false;

	cpu->setNextSyncPoint(getNext());
}


template <typename Archive>
void SynchronizationPoint::serialize(Archive& ar, unsigned /*version*/)
{
	// SynchronizationPoint is always serialized via Schedulable. A
	// Schedulable has a collection of SynchronizationPoints, all with the
	// same Schedulable. So there's no need to serialize 'device'.
	//Schedulable* device;
	ar.serialize("time", timeStamp);
	ar.serialize("type", userData);
}
INSTANTIATE_SERIALIZE_METHODS(SynchronizationPoint);

template <typename Archive>
void Scheduler::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("currentTime", scheduleTime);
	// don't serialize syncPoints, each Schedulable serializes its own
	// syncpoints
}
INSTANTIATE_SERIALIZE_METHODS(Scheduler);

} // namespace openmsx

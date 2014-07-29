#include "Scheduler.hh"
#include "Schedulable.hh"
#include "Thread.hh"
#include "MSXCPU.hh"
#include "serialize.hh"
#include <cassert>
#include <algorithm>
#include <iterator> // for back_inserter

namespace openmsx {

struct FindSchedulable {
	explicit FindSchedulable(const Schedulable& schedulable_)
		: schedulable(schedulable_) {}
	bool operator()(const SynchronizationPoint& sp) const {
		return sp.getDevice() == &schedulable;
	}
	const Schedulable& schedulable;
};

struct EqualSchedulable {
	EqualSchedulable(const Schedulable& schedulable_, int userdata_)
		: schedulable(schedulable_), userdata(userdata_) {}
	bool operator()(const SynchronizationPoint& sp) const {
		return (sp.getDevice() == &schedulable) &&
		       (sp.getUserData() == userdata);
	}
	const Schedulable& schedulable;
	int userdata;
};


Scheduler::Scheduler()
	: scheduleTime(EmuTime::zero)
	, cpu(nullptr)
	, scheduleInProgress(false)
{
}

Scheduler::~Scheduler()
{
	assert(!cpu);
	SyncPoints copy(std::begin(queue), std::end(queue));
	for (auto& s : copy) {
		s.getDevice()->schedulerDeleted();
	}

	assert(queue.empty());
}

void Scheduler::setSyncPoint(EmuTime::param time, Schedulable& device, int userData)
{
	assert(Thread::isMainThread());
	assert(time >= scheduleTime);

	// Push sync point into queue.
	queue.insert(SynchronizationPoint(time, &device, userData),
	             [](SynchronizationPoint& sp) { sp.setTime(EmuTime::infinity); },
	             [](const SynchronizationPoint& x, const SynchronizationPoint& y) {
	                     return x.getTime() < y.getTime(); });

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
	copy_if(std::begin(queue), std::end(queue), back_inserter(result),
	        FindSchedulable(device));
	return result;
}

bool Scheduler::removeSyncPoint(Schedulable& device, int userData)
{
	assert(Thread::isMainThread());
	return queue.remove(EqualSchedulable(device, userData));
}

void Scheduler::removeSyncPoints(Schedulable& device)
{
	assert(Thread::isMainThread());
	queue.remove_all(FindSchedulable(device));
}

bool Scheduler::pendingSyncPoint(const Schedulable& device, int userData) const
{
	assert(Thread::isMainThread());
	return std::find_if(std::begin(queue), std::end(queue),
	                    EqualSchedulable(device, userData)) != std::end(queue);
}

EmuTime::param Scheduler::getCurrentTime() const
{
	assert(Thread::isMainThread());
	return scheduleTime;
}

void Scheduler::scheduleHelper(EmuTime::param limit, EmuTime next)
{
	assert(!scheduleInProgress);
	scheduleInProgress = true;
	while (true) {
		assert(scheduleTime <= next);
		scheduleTime = next;

		const auto& sp = queue.front();
		auto* device = sp.getDevice();
		int userData = sp.getUserData();

		queue.remove_front();

		device->executeUntil(next, userData);

		next = getNext();
		if (likely(next > limit)) break;
	}
	scheduleInProgress = false;

	cpu->setNextSyncPoint(next);
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
	// don't serialize 'queue', each Schedulable serializes its own
	// syncpoints
}
INSTANTIATE_SERIALIZE_METHODS(Scheduler);

} // namespace openmsx

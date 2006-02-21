// $Id$

#include "EventDistributor.hh"
#include "EventListener.hh"
#include "Scheduler.hh"
#include "openmsx.hh"
#include <cassert>

using std::pair;
using std::string;

namespace openmsx {

EventDistributor::EventDistributor(Scheduler& scheduler)
	: Schedulable(scheduler)
	, sem(1)
{
}

EventDistributor::~EventDistributor()
{
	ScopedLock lock(sem);

	for (std::deque<Event*>::iterator it = scheduledEvents.begin();
	     it != scheduledEvents.end(); ++it) {
		delete *it;
	}
}

void EventDistributor::registerEventListener(
		EventType type, EventListener& listener)
{
	ScopedLock lock(sem);
	detachedListeners.insert(ListenerMap::value_type(type, &listener));
}

void EventDistributor::unregisterEventListener(
		EventType type, EventListener& listener)
{
	ScopedLock lock(sem);
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		detachedListeners.equal_range(type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		if (it->second == &listener) {
			detachedListeners.erase(it);
			break;
		}
	}
}

void EventDistributor::distributeEvent(Event* event)
{
	ScopedLock lock(sem);

	assert(event);

	// Deliver event to DETACHED listeners.
	// TODO: Implement a real solution against modifying data structure while
	//       iterating through it.
	//       For example, assign NULL first and then iterate again after
	//       delivering events to remove the NULL values.
	// TODO: Is there an easier way to search a map?
	// TODO: Is it guaranteed that the Nth entry in scheduledEvents always
	//       corresponds to the Nth call to executeUntil?
	// TODO: Is it useful to test for 0 listeners or should we just always
	//       queue the event?
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds2 =
		detachedListeners.equal_range(event->getType());
	if (bounds2.first != bounds2.second) {
		scheduledEvents.push_back(event);
		setSyncPoint(Scheduler::ASAP);
	}
}

void EventDistributor::executeUntil(const EmuTime& /*time*/, int /*userData*/)
{
	ScopedLock lock(sem);
	Event* event = scheduledEvents.front();
	scheduledEvents.pop_front();
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		detachedListeners.equal_range(event->getType());
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		sem.up();
		it->second->signalEvent(*event);
		sem.down();
	}
	delete event;
}

const string& EventDistributor::schedName() const
{
	static const string name = "EventDistributor";
	return name;
}

} // namespace openmsx

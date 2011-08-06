// $Id$

#include "EventDistributor.hh"
#include "EventListener.hh"
#include "Reactor.hh"
#include "Thread.hh"
#include "openmsx.hh"
#include <cassert>

using std::pair;
using std::string;

namespace openmsx {

EventDistributor::EventDistributor(Reactor& reactor_)
	: reactor(reactor_)
	, sem(1)
{
}

void EventDistributor::registerEventListener(
		EventType type, EventListener& listener, Priority priority)
{
	ScopedLock lock(sem);
	PriorityMap& priorityMap = listeners[type];
	for (PriorityMap::const_iterator it = priorityMap.begin();
	     it != priorityMap.end(); ++it) {
		// a listener may only be registered once for each type
		assert(it->second != &listener);
	}
	priorityMap.insert(PriorityMap::value_type(priority, &listener));
}

void EventDistributor::unregisterEventListener(
		EventType type, EventListener& listener)
{
	ScopedLock lock(sem);
	PriorityMap& priorityMap = listeners[type];
	for (PriorityMap::iterator it = priorityMap.begin();
	     it != priorityMap.end(); ++it) {
		if (it->second == &listener) {
			priorityMap.erase(it);
			break;
		}
	}
}

void EventDistributor::distributeEvent(Event* event)
{
	// TODO: Implement a real solution against modifying data structure while
	//       iterating through it.
	//       For example, assign NULL first and then iterate again after
	//       delivering events to remove the NULL values.
	// TODO: Is it useful to test for 0 listeners or should we just always
	//       queue the event?
	assert(event);
	ScopedLock lock(sem);
	if (!listeners[event->getType()].empty()) {
		// shared_ptr is not thread safe of its own, so instead of
		// passing a shared_ptr as parameter to this method we create
		// the shared_ptr here while we hold the lock
		scheduledEvents.push_back(EventPtr(event));
		// must release lock, otherwise there's a deadlock:
		//   thread 1: Reactor::deleteMotherBoard()
		//             EventDistributor::unregisterEventListener()
		//   thread 2: EventDistributor::distributeEvent()
		//             Reactor::enterMainLoop()
		cond.signalAll();
		lock.release();
		reactor.enterMainLoop();
	} else {
		delete event;
	}
}

bool EventDistributor::isRegistered(EventType type, EventListener* listener) const
{
	TypeMap::const_iterator it = listeners.find(type);
	if (it == listeners.end()) return false;

	const PriorityMap& priorityMap = it->second;
	for (PriorityMap::const_iterator it2 = priorityMap.begin();
	     it2 != priorityMap.end(); ++it2) {
		if (it2->second == listener) {
			return true;
		}
	}
	return false;
}

void EventDistributor::deliverEvents()
{
	assert(Thread::isMainThread());

	ScopedLock lock(sem);
	// It's possible that executing an event triggers scheduling of another
	// event. We also want to execute those secondary events. That's why
	// we have this while loop here.
	// For example the 'loadstate' command event, triggers a machine switch
	// event and as reaction to the latter event, AfterCommand will
	// unsubscribe from the ols MSXEventDistributor. This really should be
	// done before we exit this method.
	while (!scheduledEvents.empty()) {
		EventQueue eventsCopy;
		swap(eventsCopy, scheduledEvents);

		for (EventQueue::const_iterator it = eventsCopy.begin();
		     it != eventsCopy.end(); ++it) {
			EventPtr event = *it;
			EventType type = event->getType();
			PriorityMap priorityMapCopy = listeners[type];
			sem.up();
			unsigned allowPriorities = unsigned(-1); // all priorities
			for (PriorityMap::const_iterator it = priorityMapCopy.begin();
			     it != priorityMapCopy.end(); ++it) {
				// It's possible delivery to one of the previous
				// Listeners unregistered the current Listener.
				if (!isRegistered(type, it->second)) continue;

				unsigned currentPriority = it->first;
				if (!(currentPriority & allowPriorities)) continue;

				unsigned maskPriorities = it->second->signalEvent(event);

				assert(maskPriorities < currentPriority);
				allowPriorities &= ~maskPriorities;
			}
			sem.down();
		}
	}
}

bool EventDistributor::sleep(unsigned us)
{
	return cond.waitTimeout(us);
}

} // namespace openmsx

// $Id$

#include "EventDistributor.hh"
#include "EventListener.hh"
#include "Reactor.hh"
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
	PRT_DEBUG("EventDistributor::distributeEvent(), event of type " << event->getType() << " getting lock");
	ScopedLock lock(sem);
	PRT_DEBUG("EventDistributor::distributeEvent(), got lock");
	if (!listeners[event->getType()].empty()) {
		PRT_DEBUG("EventDistributor::distributeEvent(), there are listeners");
		// shared_ptr is not thread safe of its own, so instead of
		// passing a shared_ptr as parameter to this method we create
		// the shared_ptr here while we hold the lock
		scheduledEvents.push_back(EventPtr(event));
		// must release lock, otherwise there's a deadlock:
		//   thread 1: Reactor::deleteMotherBoard()
		//             EventDistributor::unregisterEventListener()
		//   thread 2: EventDistributor::distributeEvent()
		//             Reactor::enterMainLoop()
		PRT_DEBUG("EventDistributor::distributeEvent(), signalling all listeners");
		cond.signalAll();
		PRT_DEBUG("EventDistributor::distributeEvent(), releasing lock");
		lock.release();
		PRT_DEBUG("EventDistributor::distributeEvent(), entering main loop");
		reactor.enterMainLoop();
		PRT_DEBUG("EventDistributor::distributeEvent(), entering main loop... DONE!");
	}
}

void EventDistributor::deliverEvents()
{
	ScopedLock lock(sem);
	EventQueue eventsCopy;
	swap(eventsCopy, scheduledEvents);

	for (EventQueue::const_iterator it = eventsCopy.begin();
	     it != eventsCopy.end(); ++it) {
		EventPtr event = *it;
		PriorityMap priorityMapCopy = listeners[event->getType()];
		sem.up();
		Priority currentPriority = OTHER;
		bool stopEventDelivery = false;
		for (PriorityMap::const_iterator it = priorityMapCopy.begin();
		     it != priorityMapCopy.end(); ++it) {
			if (currentPriority != it->first) {
				currentPriority = it->first;
				if (stopEventDelivery) {
					break;
				}
			}
			if (!(it->second->signalEvent(event))) {
				// only named priorities can prohibit events
				// for lower priorities
				assert(it->first != OTHER);
				stopEventDelivery = true;
			}
		}
		sem.down();
	}
}

bool EventDistributor::sleep(unsigned us)
{
	return cond.waitTimeout(us);
}

} // namespace openmsx

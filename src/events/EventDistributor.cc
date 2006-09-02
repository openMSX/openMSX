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

EventDistributor::~EventDistributor()
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
	// named priorities can only have one listener per event
	assert((priority == OTHER) || (priorityMap.count(priority) == 0));
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

void EventDistributor::distributeEvent(EventPtr event)
{
	// TODO: Implement a real solution against modifying data structure while
	//       iterating through it.
	//       For example, assign NULL first and then iterate again after
	//       delivering events to remove the NULL values.
	// TODO: Is it useful to test for 0 listeners or should we just always
	//       queue the event?
	assert(event.get());
	ScopedLock lock(sem);
	if (!listeners[event->getType()].empty()) {
		scheduledEvents.push_back(event);
		// must release lock, otherwise there's a deadlock:
		//   thread 1: Reactor::deleteMotherBoard()
		//             EventDistributor::unregisterEventListener()
		//   thread 2: EventDistributor::distributeEvent()
		//             Reactor::enterMainLoop()
		lock.release();
		reactor.enterMainLoop();
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
		for (PriorityMap::const_iterator it = priorityMapCopy.begin();
		     it != priorityMapCopy.end(); ++it) {
			if (!(it->second->signalEvent(event))) {
				// only named priorities can prohibit events
				// for lower priorities
				assert(it->first != OTHER);
				break;
			}
		}
		sem.down();
	}
}

} // namespace openmsx

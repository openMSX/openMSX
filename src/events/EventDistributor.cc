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
	ScopedLock lock(sem);

	for (EventQueue::iterator it = scheduledEvents.begin();
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
	// TODO: Implement a real solution against modifying data structure while
	//       iterating through it.
	//       For example, assign NULL first and then iterate again after
	//       delivering events to remove the NULL values.
	// TODO: Is it useful to test for 0 listeners or should we just always
	//       queue the event?
	assert(event);
	ScopedLock lock(sem);
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds2 =
		detachedListeners.equal_range(event->getType());
	if (bounds2.first != bounds2.second) {
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
	EventQueue copy;
	swap(copy, scheduledEvents);
	
	for (EventQueue::const_iterator it = copy.begin();
	     it != copy.end(); ++it) {
		Event* event = *it;
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
}

} // namespace openmsx

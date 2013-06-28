#include "EventDistributor.hh"
#include "EventListener.hh"
#include "Reactor.hh"
#include "Thread.hh"
#include "openmsx.hh"
#include <algorithm>
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
	auto& priorityMap = listeners[type];
	for (auto& p : priorityMap) {
		// a listener may only be registered once for each type
		assert(p.second != &listener); (void)p;
	}
	priorityMap.insert(PriorityMap::value_type(priority, &listener));
}

void EventDistributor::unregisterEventListener(
		EventType type, EventListener& listener)
{
	ScopedLock lock(sem);
	auto& priorityMap = listeners[type];
	auto it = find_if(priorityMap.begin(), priorityMap.end(),
		[&](PriorityMap::value_type v) { return v.second == &listener; });
	assert(it != priorityMap.end());
	priorityMap.erase(it);
}

void EventDistributor::distributeEvent(const EventPtr& event)
{
	// TODO: Implement a real solution against modifying data structure while
	//       iterating through it.
	//       For example, assign nullptr first and then iterate again after
	//       delivering events to remove the nullptr values.
	// TODO: Is it useful to test for 0 listeners or should we just always
	//       queue the event?
	assert(event);
	ScopedLock lock(sem);
	if (!listeners[event->getType()].empty()) {
		scheduledEvents.push_back(event);
		// must release lock, otherwise there's a deadlock:
		//   thread 1: Reactor::deleteMotherBoard()
		//             EventDistributor::unregisterEventListener()
		//   thread 2: EventDistributor::distributeEvent()
		//             Reactor::enterMainLoop()
		cond.signalAll();
		lock.release();
		reactor.enterMainLoop();
	}
}

bool EventDistributor::isRegistered(EventType type, EventListener* listener) const
{
	auto it = listeners.find(type);
	if (it == listeners.end()) return false;

	for (auto& p : it->second) {
		if (p.second == listener) {
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

		for (auto& event : eventsCopy) {
			auto type = event->getType();
			auto priorityMapCopy = listeners[type];
			sem.up();
			unsigned allowPriorities = unsigned(-1); // all priorities
			for (auto& p : priorityMapCopy) {
				// It's possible delivery to one of the previous
				// Listeners unregistered the current Listener.
				if (!isRegistered(type, p.second)) continue;

				unsigned currentPriority = p.first;
				if (!(currentPriority & allowPriorities)) continue;

				unsigned maskPriorities = p.second->signalEvent(event);

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

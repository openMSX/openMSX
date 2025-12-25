#include "EventDistributor.hh"

#include "EventListener.hh"
#include "InputEventGenerator.hh"

#include "Interpreter.hh"
#include "RTScheduler.hh"
#include "Reactor.hh"
#include "Thread.hh"

#include "stl.hh"

#include <algorithm>
#include <cassert>

namespace openmsx {

EventDistributor::EventDistributor(Reactor& reactor_)
	: reactor(reactor_)
{
}

void EventDistributor::registerEventListener(
		EventType type, EventListener& listener, Priority priority)
{
	std::scoped_lock lock(mutex);
	auto& priorityMap = listeners[size_t(type)];
	// a listener may only be registered once for each type
	assert(!contains(priorityMap, &listener, &Entry::listener));
	// insert at highest position that keeps listeners sorted on priority
	auto it = std::ranges::upper_bound(priorityMap, priority, {}, &Entry::priority);
	priorityMap.emplace(it, Entry{.priority = priority, .listener = &listener});
}

void EventDistributor::unregisterEventListener(
		EventType type, EventListener& listener)
{
	std::scoped_lock lock(mutex);
	auto& priorityMap = listeners[size_t(type)];
	priorityMap.erase(rfind_unguarded(priorityMap, &listener, &Entry::listener));
}

void EventDistributor::distributeEvent(Event&& event)
{
	// TODO: Implement a real solution against modifying data structure while
	//       iterating through it.
	//       For example, assign nullptr first and then iterate again after
	//       delivering events to remove the nullptr values.
	// TODO: Is it useful to test for 0 listeners or should we just always
	//       queue the event?
	std::unique_lock lock(mutex);
	if (!listeners[size_t(getType(event))].empty()) {
		scheduledEvents.push_back(std::move(event));
		// must release lock, otherwise there's a deadlock:
		//   thread 1: Reactor::deleteMotherBoard()
		//             EventDistributor::unregisterEventListener()
		//   thread 2: EventDistributor::distributeEvent()
		//             Reactor::enterMainLoop()
		lock.unlock();
		reactor.enterMainLoop();
	}
}

bool EventDistributor::isRegistered(EventType type, EventListener* listener) const
{
	return contains(listeners[size_t(type)], listener, &Entry::listener);
}

void EventDistributor::deliverEvents(std::optional<int> timeoutMs)
{
	static PriorityMap priorityMapCopy; // static to preserve capacity
	static EventQueue eventsCopy;       // static to preserve capacity

	assert(Thread::isMainThread());

	reactor.getInputEventGenerator().poll(timeoutMs);
	reactor.getInterpreter().poll();
	reactor.getRTScheduler().execute();

	std::unique_lock lock(mutex);
	// It's possible that executing an event triggers scheduling of another
	// event. We also want to execute those secondary events. That's why
	// we have this while loop here.
	// For example the 'loadstate' command event, triggers a machine switch
	// event and as reaction to the latter event, AfterCommand will
	// unsubscribe from the ols MSXEventDistributor. This really should be
	// done before we exit this method.
	while (!scheduledEvents.empty()) {
		assert(eventsCopy.empty());
		swap(eventsCopy, scheduledEvents);
		for (const auto& event : eventsCopy) {
			auto type = getType(event);
			priorityMapCopy = listeners[size_t(type)];
			lock.unlock();
			auto allowPriority = Priority::LOWEST; // allow all
			for (const auto& e : priorityMapCopy) {
				// It's possible delivery to one of the previous
				// Listeners unregistered the current Listener.
				if (!isRegistered(type, e.listener)) continue;

				if (e.priority > allowPriority) break;

				// This might throw, e.g. when failing to initialize video system
				if (e.listener->signalEvent(event)) {
					allowPriority = e.priority;
				}
			}
			lock.lock();
		}
		eventsCopy.clear();
	}
}

} // namespace openmsx

#ifndef EVENTDISTRIBUTOR_HH
#define EVENTDISTRIBUTOR_HH

#include "Event.hh"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace openmsx {

class Reactor;
class EventListener;

class EventDistributor
{
public:
	using EventPtr = std::shared_ptr<const Event>;

	/** Priorities from high to low, higher priority listeners can block
	  * events for lower priority listeners.
	  */
	enum Priority {
		OTHER,
		CONSOLE,
		HOTKEY,
		MSX,
	};

	explicit EventDistributor(Reactor& reactor);

	/**
	 * Registers a given object to receive certain events.
	 * @param type The type of the events you want to receive.
	 * @param listener Listener that will be notified when an event arrives.
	 * @param priority Listeners have a priority, higher priority liseners
	 *                 can block events for lower priority listeners.
	 */
	void registerEventListener(EventType type, EventListener& listener,
	                           Priority priority = OTHER);

	/**
	 * Unregisters a previously registered event listener.
	 * @param type The type of the events the listener should no longer receive.
	 * @param listener Listener to unregister.
	 */
	void unregisterEventListener(EventType type, EventListener& listener);

	/** Schedule the given event for delivery. Actual delivery happens
	  * when the deliverEvents() method is called. Events are always
	  * in the main thread.
	  */
	void distributeEvent(const EventPtr& event);

	/** This actually delivers the events. It may only be called from the
	  * main loop in Reactor (and only from the main thread). Also see
	  * the distributeEvent() method.
	  */
	void deliverEvents();

	/** Sleep for the specified amount of time, but return early when
	  * (another thread) called the distributeEvent() method.
	  * @param us Amount of time to sleep, in micro seconds.
	  * @result true  if we return because time has passed
	  *         false if we return because distributeEvent() was called
	  */
	bool sleep(unsigned us);

private:
	bool isRegistered(EventType type, EventListener* listener) const;

	Reactor& reactor;

	using PriorityMap = std::vector<std::pair<Priority, EventListener*>>; // sorted on priority
	PriorityMap listeners[NUM_EVENT_TYPES];
	using EventQueue = std::vector<EventPtr>;
	EventQueue scheduledEvents;
	std::mutex mutex; // lock datastructures
	std::mutex cvMutex; // lock condition_variable
	std::condition_variable condition;
};

} // namespace openmsx

#endif

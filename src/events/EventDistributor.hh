// $Id$

#ifndef EVENTDISTRIBUTOR_HH
#define EVENTDISTRIBUTOR_HH

#include "Event.hh"
#include "Semaphore.hh"
#include "CondVar.hh"
#include "shared_ptr.hh"
#include "noncopyable.hh"
#include <map>
#include <vector>

namespace openmsx {

class Reactor;
class EventListener;

class EventDistributor : private noncopyable
{
public:
	/** Priorities from high to low, higher priority listeners can block
	  * events for lower priority listeners.
	  */
	enum Priority {
		OTHER   = 0x08,
		CONSOLE = 0x04,
		HOTKEY  = 0x02,
		MSX     = 0x01,
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
	void distributeEvent(Event* event);

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

	typedef std::multimap<Priority, EventListener*, std::greater<Priority>>
		PriorityMap; // sort from big to small
	typedef std::map<EventType, PriorityMap> TypeMap;
	TypeMap listeners;
	typedef shared_ptr<const Event> EventPtr;
	typedef std::vector<EventPtr> EventQueue;
	EventQueue scheduledEvents;
	Semaphore sem;
	CondVar cond;
};

} // namespace openmsx

#endif

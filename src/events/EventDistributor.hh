// $Id$

#ifndef EVENTDISTRIBUTOR_HH
#define EVENTDISTRIBUTOR_HH

#include "Event.hh"
#include "Semaphore.hh"
#include "shared_ptr.hh"
#include <map>
#include <vector>
#include <noncopyable.hh>

namespace openmsx {

class Reactor;
class EventListener;
class EmuTime;

class EventDistributor : private noncopyable
{
public:
	typedef shared_ptr<const Event> EventPtr;

	/** Priorities from high to low, higher priority listeners can block
	  * events for lower priority listeners.
	  */
	enum Priority {
		OTHER,
		HOTKEY,
		CONSOLE,
		MSX
	};

	explicit EventDistributor(Reactor& reactor);
	virtual ~EventDistributor();

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

	void distributeEvent(EventPtr event);
	void deliverEvents();

private:
	Reactor& reactor;

	typedef std::multimap<Priority, EventListener*> PriorityMap;
	typedef std::map<EventType, PriorityMap> TypeMap;
	TypeMap listeners;
	typedef std::vector<EventPtr> EventQueue;
	EventQueue scheduledEvents;
	Semaphore sem;
};

} // namespace openmsx

#endif

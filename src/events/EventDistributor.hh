// $Id$

#ifndef EVENTDISTRIBUTOR_HH
#define EVENTDISTRIBUTOR_HH

#include "Event.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include <map>
#include <deque>

namespace openmsx {

class EventListener;
class EmuTime;

class EventDistributor : private Schedulable
{
public:
	explicit EventDistributor(Scheduler& scheduler);
	virtual ~EventDistributor();

	/**
	 * Registers a given object to receive certain events.
	 * @param type The type of the events you want to receive.
	 * @param listener Listener that will be notified when an event arrives.
	 * @param listenerType The way this event should be delivered.
	 */
	void registerEventListener(EventType type, EventListener& listener);
	/**
	 * Unregisters a previously registered event listener.
	 * @param type The type of the events the listener should no longer receive.
	 * @param listener Listener to unregister.
	 * @param listenerType Delivery method the listener was registered with.
	 */
	void unregisterEventListener(EventType type, EventListener& listener);

	void distributeEvent(Event* event);

private:
	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	typedef std::multimap<EventType, EventListener*> ListenerMap;
	ListenerMap detachedListeners;

	typedef std::deque<Event*> EventQueue;
	EventQueue scheduledEvents;
	Semaphore sem;
};

} // namespace openmsx

#endif

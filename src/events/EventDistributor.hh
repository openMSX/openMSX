// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <map>
#include <vector>
#include <deque>
#include "Event.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "FloatSetting.hh"

namespace openmsx {

class EventListener;

class EventDistributor : private Schedulable
{
public:
	static EventDistributor& instance();

	/** Ways in which an event can be delivered to a listener.
	  */
	enum ListenerType {
		/** Deliver the event immediately, in the same thread that posted it.
		  * A native event handler can block the event from being processed
		  * by other listener types.
		  * Low overhead, but watch out for threading issues and do not
		  * unregister listeners in the callback.
		  */
		NATIVE,

		/** Deliver the event immediately, in a separate thread.
		  * A bit of overhead, but safe.
		  */
		DETACHED,

		/** Deliver the event at the EmuTime equivalent of the current real
		  * time, in a separate thread.
		  * Typically used by MSX input devices.
		  */
		EMU,
	};

	/**
	 * Registers a given object to receive certain events.
	 * @param type The type of the events you want to receive.
	 * @param listener Listener that will be notified when an event arrives.
	 * @param listenerType The way this event should be delivered.
	 */
	void   registerEventListener(EventType type, EventListener& listener,
	                             ListenerType listenerType = EMU);
	/**
	 * Unregisters a previously registered event listener.
	 * @param type The type of the events the listener should no longer receive.
	 * @param listener Listener to unregister.
	 * @param listenerType Delivery method the listener was registered with.
	 */
	void unregisterEventListener(EventType type, EventListener& listener,
	                             ListenerType listenerType = EMU);

	void distributeEvent(Event* event);
	void sync(const EmuTime& emuTime);
	
private:
	EventDistributor();
	virtual ~EventDistributor();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	typedef std::multimap<EventType, EventListener*> ListenerMap;
	ListenerMap nativeListeners;
	ListenerMap detachedListeners;
	ListenerMap emuListeners;
	ListenerMap& getListeners(ListenerType listenerType);

	struct EventTime {
		EventTime(Event* event_, unsigned long long time_)
			: event(event_), time(time_) {}
		Event* event;
		unsigned long long time;
	};
	std::vector<EventTime> toBeScheduledEvents;
	std::deque<Event*> scheduledEvents;

	EmuTime prevEmu;
	unsigned long long prevReal;
	//FloatSetting delaySetting;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__

// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <map>
#include "Event.hh"

using std::multimap;

namespace openmsx {

class EventListener;

class EventDistributor
{
public:
	static EventDistributor& instance();

	/**
	 * Use this method to (un)register a given class to receive certain
	 * events.
	 * @param type The type of the events you want to receive
	 * @param listener Object that will be notified when the events arrives
	 * @param priority The priority of the listener (lower number is higher
	 *        priority). Higher priority listeners may block an event for
	 *        lower priority listeners. Normally you don't need to specify
	 *        a priority.
	 *        Note: in the current implementation there are only two
	 *              priority levels (0 and !=0)
	 * The delivery of the event is done by the 'main-emulation-thread',
	 * so there is no need for extra synchronization.
	 */
	void   registerEventListener(EventType type, EventListener& listener, int priority = 0);
	void unregisterEventListener(EventType type, EventListener& listener, int priority = 0);

	void distributeEvent(const Event& event);
	
private:
	EventDistributor();
	virtual ~EventDistributor();

	typedef multimap<EventType, EventListener*> ListenerMap;
	ListenerMap lowMap;
	ListenerMap highMap;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__

#ifndef EVENTLISTENER_HH
#define EVENTLISTENER_HH

#include "Event.hh"

namespace openmsx {

class EventListener
{
public:
	EventListener(const EventListener&) = delete;
	EventListener(EventListener&&) = delete;
	EventListener& operator=(const EventListener&) = delete;
	EventListener& operator=(EventListener&&) = delete;

	/**
	 * This method gets called when an event you are subscribed to occurs.
	 * @result Must return a bitmask of EventListener priorities. When a
	 *         bit is set, this event won't be delivered to listeners with
	 *         that priority. It's only allowed/possible to block an event
	 *         for listeners with a strictly lower priority than this
	 *         listener. Returning 0 means don't block the event for any
	 *         listeners.
	 */
	virtual int signalEvent(const Event& event) = 0;

protected:
	EventListener() = default;
	~EventListener() = default;
};

} // namespace openmsx

#endif // EVENTLISTENER_HH

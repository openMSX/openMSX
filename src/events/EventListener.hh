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
	 * @result Return true when this event must be blocked from listeners
	 *         with a (strictly) lower priority.
	 */
	virtual bool signalEvent(const Event& event) = 0;

protected:
	EventListener() = default;
	~EventListener() = default;
};

} // namespace openmsx

#endif // EVENTLISTENER_HH

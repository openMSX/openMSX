// $Id$

#ifndef EVENTLISTENER_HH
#define EVENTLISTENER_HH

namespace openmsx {

class Event;

class EventListener
{
public:
	virtual ~EventListener() {}

	/**
	 * This method gets called when an event you are interested in
	 * occurs.
	 * This method should return true when lower priority
	 * EventListener may also receive this event (normally always
	 * the case except for Console)
	 */
	virtual bool signalEvent(const Event& event) = 0;

protected:
	EventListener() {}
};

} // namespace openmsx

#endif

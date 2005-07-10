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
	 * This method gets called when an event you are subscribed to occurs.
	 */
	virtual void signalEvent(const Event& event) = 0;

protected:
	EventListener() {}
};

} // namespace openmsx

#endif // EVENTLISTENER_HH

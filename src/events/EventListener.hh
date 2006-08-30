// $Id$

#ifndef EVENTLISTENER_HH
#define EVENTLISTENER_HH

#include "shared_ptr.hh"

namespace openmsx {

class Event;

class EventListener
{
public:
	virtual ~EventListener() {}

	/**
	 * This method gets called when an event you are subscribed to occurs.
	 */
	virtual void signalEvent(shared_ptr<const Event> event) = 0;

protected:
	EventListener() {}
};

} // namespace openmsx

#endif // EVENTLISTENER_HH

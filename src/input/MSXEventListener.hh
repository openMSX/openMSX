// $Id$

#ifndef MSXEVENTLISTENER_HH
#define MSXEVENTLISTENER_HH

#include "shared_ptr.hh"

namespace openmsx {

class Event;
class EmuTime;

class MSXEventListener
{
public:
	virtual ~MSXEventListener() {}

	/** This method gets called when an event you are subscribed to occurs.
	  */
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time) = 0;

protected:
	MSXEventListener() {}
};

} // namespace openmsx

#endif

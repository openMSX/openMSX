// $Id$

#ifndef MSXEVENTLISTENER_HH
#define MSXEVENTLISTENER_HH

#include "EmuTime.hh"
#include "shared_ptr.hh"

namespace openmsx {

class Event;

class MSXEventListener
{
public:
	virtual ~MSXEventListener() {}

	/** This method gets called when an event you are subscribed to occurs.
	  */
	virtual void signalEvent(const shared_ptr<const Event>& event,
	                         EmuTime::param time) = 0;

protected:
	MSXEventListener() {}
};

} // namespace openmsx

#endif

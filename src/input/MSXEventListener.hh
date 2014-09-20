#ifndef MSXEVENTLISTENER_HH
#define MSXEVENTLISTENER_HH

#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class Event;

class MSXEventListener
{
public:
	/** This method gets called when an event you are subscribed to occurs.
	  */
	virtual void signalEvent(const std::shared_ptr<const Event>& event,
	                         EmuTime::param time) = 0;

protected:
	MSXEventListener() {}
	~MSXEventListener() {}
};

} // namespace openmsx

#endif

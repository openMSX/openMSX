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
	virtual void signalMSXEvent(const Event& event,
	                            EmuTime::param time) noexcept = 0;

protected:
	MSXEventListener() = default;
	~MSXEventListener() = default;
};

} // namespace openmsx

#endif

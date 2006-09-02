// $Id$

#ifndef ICONSTATUS_HH
#define ICONSTATUS_HH

#include "EventListener.hh"
#include "LedEvent.hh"
#include "noncopyable.hh"

namespace openmsx {

class EventDistributor;

class IconStatus : private EventListener, private noncopyable
{
public:
	IconStatus(EventDistributor& eventDistributor);
	~IconStatus();

	bool getStatus(int icon) const;
	unsigned long long getTime(int icon) const;

private:
	// EventListener interface:
	virtual bool signalEvent(shared_ptr<const Event> event);

	EventDistributor& eventDistributor;
	bool iconStatus[LedEvent::NUM_LEDS];
	unsigned long long iconTime[LedEvent::NUM_LEDS];
};

} // namespace openmsx

#endif

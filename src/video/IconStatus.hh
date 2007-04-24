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
	explicit IconStatus(EventDistributor& eventDistributor);
	~IconStatus();

	bool getStatus(int icon) const;
	unsigned long long getTime(int icon) const;

private:
	// EventListener interface:
	virtual bool signalEvent(shared_ptr<const Event> event);

	EventDistributor& eventDistributor;
	unsigned long long iconTime[LedEvent::NUM_LEDS];
	bool iconStatus[LedEvent::NUM_LEDS];
};

} // namespace openmsx

#endif

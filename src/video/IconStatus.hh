// $Id$

#ifndef ICONSTATUS_HH
#define ICONSTATUS_HH

#include "EventListener.hh"
#include "LedEvent.hh"
#include "noncopyable.hh"

namespace openmsx {

class EventDistributor;
class Display;

class IconStatus : private EventListener, private noncopyable
{
public:
	IconStatus(EventDistributor& eventDistributor, Display& display);
	~IconStatus();

	bool getStatus(int icon) const;
	unsigned long long getTime(int icon) const;

private:
	// EventListener interface:
	virtual void signalEvent(const Event& event);

	EventDistributor& eventDistributor;
	Display& display;
	bool iconStatus[LedEvent::NUM_LEDS];
	unsigned long long iconTime[LedEvent::NUM_LEDS];
};

} // namespace openmsx

#endif

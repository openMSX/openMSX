// $Id$

#ifndef _LEDEVENT_HH_
#define _LEDEVENT_HH_

#include "Event.hh"

namespace openmsx {

class LedEvent : public Event
{
public:
	enum Led {
		POWER,
		CAPS,
		KANA, // same as CODE LED
		PAUSE,
		TURBO,
		FDD,
	};

	LedEvent(Led led_, bool status_)
		: Event(LED_EVENT)
		, led(led_)
		, status(status_) {}

	Led getLed() const { return led; }
	bool getStatus() const { return status; }

private:
	Led led;
	bool status;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__

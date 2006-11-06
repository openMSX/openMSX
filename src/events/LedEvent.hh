// $Id$

#ifndef LEDEVENT_HH
#define LEDEVENT_HH

#include "Event.hh"
#include <string>

namespace openmsx {

class MSXMotherBoard;

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
		NUM_LEDS // must be last
	};

	LedEvent(Led led, bool status, MSXMotherBoard& motherBoard);

	Led getLed() const;
	bool getStatus() const;
	const std::string& getMachine() const;

private:
	Led led;
	bool status;
	std::string machine;
};

} // namespace openmsx

#endif

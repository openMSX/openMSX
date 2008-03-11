// $Id$

#include "LedEvent.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

LedEvent::LedEvent(Led led_, bool status_, MSXMotherBoard& motherBoard)
	: Event(OPENMSX_LED_EVENT)
	, led(led_)
	, status(status_)
{
	machine = motherBoard.getMachineID();
}

LedEvent::Led LedEvent::getLed() const
{
	return led;
}

bool LedEvent::getStatus() const
{
	return status;
}

const std::string& LedEvent::getMachine() const
{
	return machine;
}

std::string LedEvent::getLedName(Led led)
{
	static const char* const names[NUM_LEDS] = {
		"power", "caps", "kana", "pause", "turbo", "FDD"
	};
	return names[led];
}

} // namespace openmsx

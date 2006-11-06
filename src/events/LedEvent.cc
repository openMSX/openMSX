// $Id: $

#include "LedEvent.hh"
#include "MSXMotherBoard.hh"
#include "MSXCommandController.hh"

namespace openmsx {

LedEvent::LedEvent(Led led_, bool status_, MSXMotherBoard& motherBoard)
	: Event(OPENMSX_LED_EVENT)
	, led(led_)
	, status(status_)
{
	machine = motherBoard.getMSXCommandController().getNamespace();
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

} // namespace openmsx

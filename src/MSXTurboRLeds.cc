// $Id$

#include "MSXTurboRLeds.hh"
#include "LedEvent.hh"
#include "EventDistributor.hh"

namespace openmsx {

MSXTurboRLeds::MSXTurboRLeds(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	reset(time);
}

MSXTurboRLeds::~MSXTurboRLeds()
{
}

void MSXTurboRLeds::reset(const EmuTime& time)
{
	writeIO(0, 0, time);
}

void MSXTurboRLeds::powerDown(const EmuTime& time)
{
	writeIO(0, 0, time);
}

void MSXTurboRLeds::writeIO(byte /*port*/, byte value, const EmuTime& /*time*/)
{
	EventDistributor::instance().distributeEvent(
		new LedEvent(LedEvent::PAUSE, value & 0x01));
	EventDistributor::instance().distributeEvent(
		new LedEvent(LedEvent::TURBO, value & 0x80));
}

} // namespace openmsx

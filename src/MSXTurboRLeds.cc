// $Id$

#include "MSXTurboRLeds.hh"
#include "Leds.hh"

namespace openmsx {

MSXTurboRLeds::MSXTurboRLeds(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	turborPaused = false;
	reset(time);
}

MSXTurboRLeds::~MSXTurboRLeds()
{
}

void MSXTurboRLeds::reset(const EmuTime& time)
{
	writeIO(0, 0, time);
}

void MSXTurboRLeds::writeIO(byte /*port*/, byte value, const EmuTime& /*time*/)
{
	if (!turborPaused) {
		if (value & 0x01) {
			Leds::instance().setLed(Leds::PAUSE_ON);
			turborPaused = true;
		}
	} else {
		if (!(value & 0x01)) {
			Leds::instance().setLed(Leds::PAUSE_OFF);
			turborPaused = false;
		}
	}
	Leds::instance().setLed((value & 0x80) ? Leds::TURBO_ON : Leds::TURBO_OFF);
}

} // namespace openmsx

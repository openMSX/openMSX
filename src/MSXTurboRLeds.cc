// $Id$

#include "MSXTurboRLeds.hh"
#include "Leds.hh"


namespace openmsx {

MSXTurboRLeds::MSXTurboRLeds(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	reset(time);
}

MSXTurboRLeds::~MSXTurboRLeds()
{
}

void MSXTurboRLeds::reset(const EmuTime &time)
{
	writeIO(0, 0, time);
}

void MSXTurboRLeds::writeIO(byte port, byte value, const EmuTime &time)
{
	Leds::instance()->setLed((value & 0x01) ? Leds::PAUSE_ON : Leds::PAUSE_OFF);
	Leds::instance()->setLed((value & 0x80) ? Leds::TURBO_ON : Leds::TURBO_OFF);
}

} // namespace openmsx

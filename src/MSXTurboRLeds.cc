// $Id$

#include "MSXTurboRLeds.hh"
#include "MSXCPUInterface.hh"
#include "Leds.hh"


MSXTurboRLeds::MSXTurboRLeds(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_Out(0xA7,this);
	reset(time);
}

MSXTurboRLeds::~MSXTurboRLeds()
{
}
 
void MSXTurboRLeds::reset(const EmuTime &time)
{
	writeIO(0xA7, 0, time);
}

void MSXTurboRLeds::writeIO(byte port, byte value, const EmuTime &time)
{
	Leds::instance()->setLed((value & 0x01) ? Leds::PAUSE_ON : Leds::PAUSE_OFF);
	Leds::instance()->setLed((value & 0x80) ? Leds::TURBO_ON : Leds::TURBO_OFF);
}

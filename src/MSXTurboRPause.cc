// $Id$

#include "MSXTurboRPause.hh"

namespace openmsx {

MSXTurboRPause::MSXTurboRPause(Config *config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time),
		turboRPauseSetting("turborpause", 
				"status of the TurboR pause", false)
{
	reset(time);
}

MSXTurboRPause::~MSXTurboRPause()
{
}
 
void MSXTurboRPause::reset(const EmuTime& time)
{
	turboRPauseSetting.setValue(false);
}

byte MSXTurboRPause::readIO(byte port, const EmuTime& time)
{
	if (turboRPauseSetting.getValue()) 
		return (byte) 0x01; 
	else
		return (byte) 0x00;
}

} // namespace openmsx

// $Id$

#include "MSXTurboRPause.hh"

namespace openmsx {

MSXTurboRPause::MSXTurboRPause(const XMLElement& config, const EmuTime& time)
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
	return turboRPauseSetting.getValue() ? 1 : 0;
}

} // namespace openmsx

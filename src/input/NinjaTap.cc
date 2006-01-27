// $Id$

#include "NinjaTap.hh"
#include "JoystickPort.hh"

namespace openmsx {

NinjaTap::NinjaTap(PluggingController& pluggingController,
                   const std::string& name)
	: JoyTap(pluggingController, name)
{
}

const std::string& NinjaTap::getDescription() const
{
	static const std::string desc("MSX NinjaTap device.");
	return desc;
}

byte NinjaTap::read(const EmuTime& time)
{
	// TODO: actually implement the ninja tap logic ! 
	byte value = 255;
	for (int i = 0; i < 4; ++i) {
		value &= slaves[i]->read(time);
	}
	return value;
}

void NinjaTap::write(byte value, const EmuTime& time)
{
	// TODO: actually implement the ninja tap logic ! 
	for (int i = 0; i < 4; ++i) {
		slaves[i]->write(value, time);
	}
}

} // namespace openmsx

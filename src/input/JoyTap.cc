// $Id$

#include "JoyTap.hh"
#include "JoystickPort.hh"

namespace openmsx {

JoyTap::JoyTap(PluggingController& pluggingController,
               const std::string& name_)
	: name(name_)
{
	for (int i = 0; i < 4; ++i) {
		slaves[i].reset(new JoystickPort(
			pluggingController,
			getName() + "_port_" + char('1' + i)));
	}
}

JoyTap::~JoyTap()
{
}

const std::string& JoyTap::getDescription() const
{
	static const std::string desc("MSX JoyTap device.");
	return desc;
}

const std::string& JoyTap::getName() const
{
	return name;
}

void JoyTap::plugHelper(Connector& /*connector*/, const EmuTime& /*time*/)
{
	// TODO move create of the joystickports to here???
}

void JoyTap::unplugHelper(const EmuTime& /*time*/)
{
	// TODO move destruction of the joystickports to here???
}

byte JoyTap::read(const EmuTime& time)
{
	byte value = 255;
	for (int i = 0; i < 4; ++i) {
		value &= slaves[i]->read(time);
	}
	return value;
}

void JoyTap::write(byte value, const EmuTime& time)
{
	for (int i = 0; i < 4; ++i) {
		slaves[i]->write(value, time);
	}
}

} // namespace openmsx

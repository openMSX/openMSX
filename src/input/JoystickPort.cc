// $Id$

#include "JoystickPort.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"


namespace openmsx {

JoystickPort::JoystickPort(const string &name)
	: Connector(name, new DummyJoystick())
{
	lastValue = 255;	// != 0
	PluggingController::instance()->registerConnector(this);
}

JoystickPort::~JoystickPort()
{
	PluggingController::instance()->unregisterConnector(this);
}

const string &JoystickPort::getClass() const
{
	static const string className("Joystick Port");
	return className;
}

void JoystickPort::plug(Pluggable *device, const EmuTime &time)
	throw(PlugException)
{
	Connector::plug(device, time);
	((JoystickDevice*)pluggable)->write(lastValue, time);
}

byte JoystickPort::read(const EmuTime &time)
{
	return ((JoystickDevice*)pluggable)->read(time);
}

void JoystickPort::write(byte value, const EmuTime &time)
{
	if (lastValue != value) {
		lastValue = value;
		((JoystickDevice*)pluggable)->write(value, time);
	}
}

} // namespace openmsx


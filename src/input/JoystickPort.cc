// $Id$

#include "JoystickPort.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"

using std::string;

namespace openmsx {

JoystickPort::JoystickPort(const string &name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyJoystick()))
{
	lastValue = 255;	// != 0
	PluggingController::instance().registerConnector(this);
}

JoystickPort::~JoystickPort()
{
	PluggingController::instance().unregisterConnector(this);
}

const string& JoystickPort::getDescription() const
{
	static const string desc("MSX Joystick port.");
	return desc;
}

const string& JoystickPort::getClass() const
{
	static const string className("Joystick Port");
	return className;
}

JoystickDevice& JoystickPort::getPlugged() const
{
	return static_cast<JoystickDevice&>(*plugged);
}

void JoystickPort::plug(Pluggable *device, const EmuTime &time)
{
	Connector::plug(device, time);
	getPlugged().write(lastValue, time);
}

byte JoystickPort::read(const EmuTime &time)
{
	return getPlugged().read(time);
}

void JoystickPort::write(byte value, const EmuTime &time)
{
	if (lastValue != value) {
		lastValue = value;
		getPlugged().write(value, time);
	}
}

} // namespace openmsx


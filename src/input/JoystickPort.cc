// $Id$

#include "JoystickPort.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"

using std::string;

namespace openmsx {

JoystickPort::JoystickPort(PluggingController& pluggingController_,
                           const string& name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyJoystick()))
	, pluggingController(pluggingController_)
	, lastValue(255) // != 0
{
	pluggingController.registerConnector(*this);
}

JoystickPort::~JoystickPort()
{
	pluggingController.unregisterConnector(*this);
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

JoystickDevice& JoystickPort::getPluggedJoyDev() const
{
	return *checked_cast<JoystickDevice*>(&getPlugged());
}

void JoystickPort::plug(Pluggable& device, const EmuTime& time)
{
	Connector::plug(device, time);
	getPluggedJoyDev().write(lastValue, time);
}

byte JoystickPort::read(const EmuTime& time)
{
	return getPluggedJoyDev().read(time);
}

void JoystickPort::write(byte value, const EmuTime& time)
{
	if (lastValue != value) {
		lastValue = value;
		getPluggedJoyDev().write(value, time);
	}
}

} // namespace openmsx


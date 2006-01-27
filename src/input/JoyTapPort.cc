// $Id$

#include "JoyTapPort.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"

using std::string;

namespace openmsx {

JoyTapPort::JoyTapPort(PluggingController& pluggingController_,
                           const string& name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyJoystick()))
	, pluggingController(pluggingController_)
	, lastValue(255) // != 0
{
	pluggingController.registerConnector(*this);
}

JoyTapPort::~JoyTapPort()
{
	pluggingController.unregisterConnector(*this);
}

const string& JoyTapPort::getDescription() const
{
	static const string desc("MSX JoyTap input connector.");
	return desc;
}

const string& JoyTapPort::getClass() const
{
//	Needs to be "Joystick Port" for the joystickdevice plugables to work
//
//	static const string className("JoyTap Port");
	static const string className("Joystick Port");
	return className;
}

JoystickDevice& JoyTapPort::getPlugged() const
{
	return static_cast<JoystickDevice&>(*plugged);
}

void JoyTapPort::plug(Pluggable& device, const EmuTime& time)
{
	Connector::plug(device, time);
	getPlugged().write(lastValue, time);
}

byte JoyTapPort::read(const EmuTime& time)
{
	return getPlugged().read(time);
}

void JoyTapPort::write(byte value, const EmuTime& time)
{
	if (lastValue != value) {
		lastValue = value;
		getPlugged().write(value, time);
	}
}

} // namespace openmsx


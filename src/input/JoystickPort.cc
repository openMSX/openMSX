// $Id$

#include "JoystickPort.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "checked_cast.hh"
#include "serialize.hh"

using std::string;

namespace openmsx {

JoystickPort::JoystickPort(PluggingController& pluggingController_,
                           const string& name)
	: Connector(pluggingController_, name,
	            std::auto_ptr<Pluggable>(new DummyJoystick()))
	, lastValue(255) // != 0
{
}

JoystickPort::~JoystickPort()
{
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

void JoystickPort::plug(Pluggable& device, EmuTime::param time)
{
	Connector::plug(device, time);
	getPluggedJoyDev().write(lastValue, time);
}

byte JoystickPort::read(EmuTime::param time)
{
	return getPluggedJoyDev().read(time);
}

void JoystickPort::write(byte value, EmuTime::param time)
{
	if (lastValue != value) {
		lastValue = value;
		getPluggedJoyDev().write(value, time);
	}
}

template<typename Archive>
void JoystickPort::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
	// don't serialize 'lastValue', done in MSXPSG
}
INSTANTIATE_SERIALIZE_METHODS(JoystickPort);

} // namespace openmsx


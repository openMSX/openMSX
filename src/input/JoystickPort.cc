// $Id$

#include "JoystickPort.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <cctype>

using std::string;

namespace openmsx {

JoystickPort::JoystickPort(PluggingController& pluggingController_,
                           string_ref name, const string& description_)
	: Connector(pluggingController_, name,
	            std::unique_ptr<Pluggable>(new DummyJoystick()))
	, lastValue(255) // != 0
	, description(description_)
{
}

JoystickPort::~JoystickPort()
{
}

const string JoystickPort::getDescription() const
{
	return description;
}

string_ref JoystickPort::getClass() const
{
	return "Joystick Port";
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


// class DummyJoystickPort

byte DummyJoystickPort::read(EmuTime::param /*time*/)
{
	return 0x3F; // do as-if nothing is connected
}

void DummyJoystickPort::write(byte /*value*/, EmuTime::param /*time*/)
{
	// ignore writes
}

} // namespace openmsx


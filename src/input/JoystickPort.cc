#include "JoystickPort.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "memory.hh"

using std::string;

namespace openmsx {

JoystickPort::JoystickPort(PluggingController& pluggingController_,
                           string_ref name_, const string& description_)
	: Connector(pluggingController_, name_, make_unique<DummyJoystick>())
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
	if (lastValue != value) writeDirect(value, time);
}
void JoystickPort::writeDirect(byte value, EmuTime::param time)
{
	lastValue = value;
	getPluggedJoyDev().write(value, time);
}

template<typename Archive>
void JoystickPort::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
	if (ar.isLoader()) {
		// The value of 'lastValue', is already restored via MSXPSG,
		// but we still need to re-write this value to the plugged
		// devices (do this after those devices have been re-plugged).
		writeDirect(lastValue, getPluggingController().getCurrentTime());
	}
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

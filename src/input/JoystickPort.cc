#include "JoystickPort.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

JoystickPort::JoystickPort(PluggingController& pluggingController_,
                           std::string name_, std::string description_)
	: Connector(pluggingController_, std::move(name_), std::make_unique<DummyJoystick>())
	, description(std::move(description_))
{
}

std::string_view JoystickPort::getDescription() const
{
	return description;
}

std::string_view JoystickPort::getClass() const
{
	return "Joystick Port";
}

JoystickDevice& JoystickPort::getPluggedJoyDev() const
{
	return *checked_cast<JoystickDevice*>(&getPlugged());
}

void JoystickPort::plug(Pluggable& device, EmuTime time)
{
	Connector::plug(device, time);
	getPluggedJoyDev().write(lastValue, time);
}

uint8_t JoystickPort::read(EmuTime time)
{
	return getPluggedJoyDev().read(time);
}

void JoystickPort::write(uint8_t value, EmuTime time)
{
	if (lastValue != value) writeDirect(value, time);
}
void JoystickPort::writeDirect(uint8_t value, EmuTime time)
{
	lastValue = value;
	getPluggedJoyDev().write(value, time);
}

template<typename Archive>
void JoystickPort::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
	if constexpr (Archive::IS_LOADER) {
		// The value of 'lastValue', is already restored via MSXPSG,
		// but we still need to re-write this value to the plugged
		// devices (do this after those devices have been re-plugged).
		writeDirect(lastValue, getPluggingController().getCurrentTime());
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoystickPort);


// class DummyJoystickPort

uint8_t DummyJoystickPort::read(EmuTime /*time*/)
{
	return 0x3F; // do as-if nothing is connected
}

void DummyJoystickPort::write(uint8_t /*value*/, EmuTime /*time*/)
{
	// ignore writes
}

} // namespace openmsx

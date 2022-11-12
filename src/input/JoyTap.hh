#ifndef JOYTAP_HH
#define JOYTAP_HH

#include "JoystickDevice.hh"
#include "JoystickPort.hh"
#include "serialize_meta.hh"
#include "static_string_view.hh"
#include <array>
#include <optional>

namespace openmsx {

class PluggingController;

/** This device is plugged in into the joy-ports and consolidates several other
 * joysticks plugged into it. This JoyTap simply ANDs all the joystick
 * outputs, acting as a simple wiring of all digital joysticks into one
 * connector.
 * This is the base class for the NinjaTap device and the FNano2 multiplayer
 * extension, who basically have other read and write methods
 */
class JoyTap : public JoystickDevice
{
public:
	JoyTap(PluggingController& pluggingController, std::string name);

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	void createPorts(static_string_view description, EmuTime::param time);

protected:
	std::array<std::optional<JoystickPort>, 4> slaves;
	PluggingController& pluggingController;

private:
	const std::string name;
};

REGISTER_BASE_NAME_HELPER(JoyTap, "JoyTap");

} // namespace openmsx

#endif

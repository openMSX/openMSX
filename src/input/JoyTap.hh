#ifndef JOYTAP_HH
#define JOYTAP_HH

#include "JoystickDevice.hh"
#include "JoystickPort.hh"
#include "serialize_meta.hh"
#include <array>
#include <optional>
#include <string_view>

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
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	void createPorts(std::string_view description, EmuTime time);

protected:
	std::array<std::optional<JoystickPort>, 4> slaves;
	PluggingController& pluggingController;

private:
	const std::string name;
};

REGISTER_BASE_NAME_HELPER(JoyTap, "JoyTap");

} // namespace openmsx

#endif

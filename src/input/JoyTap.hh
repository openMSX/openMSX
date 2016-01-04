#ifndef JOYTAP_HH
#define JOYTAP_HH

#include "JoystickDevice.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class PluggingController;
class JoystickPort;

/** This device is pluged in into the joyports and consolidates several other
 * joysticks plugged into it. This joytap simply ANDs all the joystick
 * outputs, acting as a simple wiring of all digital joysticks into one
 * connector.
 * This is the base class for the NinjaTap device and the FNano2 multiplayer
 * extension, who basicly have other read and write methods
 */
class JoyTap : public JoystickDevice
{
public:
	JoyTap(PluggingController& pluggingController,
	       const std::string& name);
	virtual ~JoyTap();

	// Pluggable
	const std::string& getName() const override;
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	void createPorts(const std::string& baseDescription);

	std::unique_ptr<JoystickPort> slaves[4];
	PluggingController& pluggingController;

private:
	const std::string name;
};

REGISTER_BASE_NAME_HELPER(JoyTap, "JoyTap");

} // namespace openmsx

#endif

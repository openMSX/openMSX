#ifndef DUMMYJOYSTICK_HH
#define DUMMYJOYSTICK_HH

#include "JoystickDevice.hh"

namespace openmsx {

class DummyJoystick final : public JoystickDevice
{
public:
	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
};

} // namespace openmsx

#endif

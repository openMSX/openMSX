#ifndef DUMMYJOYSTICK_HH
#define DUMMYJOYSTICK_HH

#include "JoystickDevice.hh"

namespace openmsx {

class DummyJoystick final : public JoystickDevice
{
public:
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
};

} // namespace openmsx

#endif

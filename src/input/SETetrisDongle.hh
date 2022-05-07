#ifndef SETETRISDONGLE_HH
#define SETETRISDONGLE_HH

#include "JoystickDevice.hh"

namespace openmsx {

class SETetrisDongle final : public JoystickDevice
{
public:
	SETetrisDongle();

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	byte status;
};

} // namespace openmsx

#endif

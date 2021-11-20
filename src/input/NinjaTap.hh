#ifndef NINJATAP_HH
#define NINJATAP_HH

#include "JoyTap.hh"

namespace openmsx {

class NinjaTap final : public JoyTap
{
public:
	NinjaTap(PluggingController& pluggingController, std::string name);

	// Pluggable
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte status;
	byte previous;
	byte buf[4];
};

} // namespace openmsx

#endif

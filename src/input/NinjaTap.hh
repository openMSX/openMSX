#ifndef NINJATAP_HH
#define NINJATAP_HH

#include "JoyTap.hh"
#include <array>

namespace openmsx {

class NinjaTap final : public JoyTap
{
public:
	NinjaTap(PluggingController& pluggingController, std::string name);

	// Pluggable
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint8_t status;
	uint8_t previous;
	std::array<uint8_t, 4> buf;
};

} // namespace openmsx

#endif

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
	void plugHelper(Connector& connector, EmuTime time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint8_t status = 0x3F; // TODO check initial value
	uint8_t previous = 0;
	std::array<uint8_t, 4> buf;
};

} // namespace openmsx

#endif

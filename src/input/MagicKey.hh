#ifndef MAGICKEY_HH
#define MAGICKEY_HH

#include "JoystickDevice.hh"

namespace openmsx {

class MagicKey final : public JoystickDevice
{
public:
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
};

} // namespace openmsx

#endif

#ifndef MSXVICTORHC9XSYSTEMCONTROL_HH
#define MSXVICTORHC9XSYSTEMCONTROL_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXVictorHC9xSystemControl final : public MSXDevice
{
public:
	explicit MSXVictorHC9xSystemControl(const DeviceConfig& config);

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] bool allowUnaligned() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte systemControlRegister = 0x80;
};

} // namespace openmsx

#endif

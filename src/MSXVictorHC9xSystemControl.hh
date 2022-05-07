#ifndef MSXVICTORHC9XSYSTEMCONTROL_HH
#define MSXVICTORHC9XSYSTEMCONTROL_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXVictorHC9xSystemControl final : public MSXDevice
{
public:
	explicit MSXVictorHC9xSystemControl(const DeviceConfig& config);

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] bool allowUnaligned() const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	byte systemControlRegister;
};

} // namespace openmsx

#endif

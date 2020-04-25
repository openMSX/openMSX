#ifndef MSXVICTORHC9XSYSTEMCONTROL_HH
#define MSXVICTORHC9XSYSTEMCONTROL_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXVictorHC9xSystemControl final : public MSXDevice
{
public:
	explicit MSXVictorHC9xSystemControl(const DeviceConfig& config);

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	bool allowUnaligned() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte systemControlRegister;
};

} // namespace openmsx

#endif

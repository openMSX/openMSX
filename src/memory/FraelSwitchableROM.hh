#ifndef FRAELSWITCHABLEROM_HH
#define FRAELSWITCHABLEROM_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class FraelSwitchableROM final : public MSXDevice
{
public:
	explicit FraelSwitchableROM(DeviceConfig& DeviceConfig);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;

	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom basicBiosRom;
	Rom firmwareRom;
	bool firmwareSelected;
};

} // namespace openmsx

#endif

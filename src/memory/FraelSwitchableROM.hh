#ifndef FRAELSWITCHABLEROM_HH
#define FRAELSWITCHABLEROM_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class FraelSwitchableROM final : public MSXDevice
{
public:
	explicit FraelSwitchableROM(DeviceConfig& DeviceConfig);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;

	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom basicBiosRom;
	Rom firmwareRom;
	bool firmwareSelected;
};

} // namespace openmsx

#endif

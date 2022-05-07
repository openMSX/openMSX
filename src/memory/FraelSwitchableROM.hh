#ifndef FRAELSWITCHABLEROM_HH
#define FRAELSWITCHABLEROM_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class FraelSwitchableROM final : public MSXDevice
{
public:
	explicit FraelSwitchableROM(const DeviceConfig& DeviceConfig);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;

	void writeIO(word port, byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	Rom basicbiosRom;
	Rom firmwareRom;
	bool firmwareSelected;
};

} // namespace openmsx

#endif

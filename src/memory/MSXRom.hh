#ifndef MSXROM_HH
#define MSXROM_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXRom : public MSXDevice
{
public:
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void getExtraDeviceInfo(TclObject& result) const override;

protected:
	MSXRom(const DeviceConfig& config, Rom&& rom);

protected:
	Rom rom;
};

} // namespace openmsx

#endif

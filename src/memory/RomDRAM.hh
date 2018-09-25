#ifndef ROMDRAM_HH
#define ROMDRAM_HH

#include "MSXRom.hh"

namespace openmsx {

class PanasonicMemory;

class RomDRAM final : public MSXRom
{
public:
	RomDRAM(const DeviceConfig& config, Rom&& rom);

	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;

private:
	PanasonicMemory& panasonicMemory;
	const unsigned baseAddr;
};

} // namespace openmsx

#endif

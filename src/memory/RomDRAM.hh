#ifndef ROMDRAM_HH
#define ROMDRAM_HH

#include "MSXRom.hh"

namespace openmsx {

class PanasonicMemory;

class RomDRAM final : public MSXRom
{
public:
	RomDRAM(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;

private:
	PanasonicMemory& panasonicMemory;
	const unsigned baseAddr;
};

} // namespace openmsx

#endif

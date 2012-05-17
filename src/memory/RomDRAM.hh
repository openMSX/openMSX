// $Id$

#ifndef ROMDRAM_HH
#define ROMDRAM_HH

#include "MSXRom.hh"

namespace openmsx {

class PanasonicMemory;

class RomDRAM : public MSXRom
{
public:
	RomDRAM(const DeviceConfig& config, std::auto_ptr<Rom> rom);

	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;

private:
	PanasonicMemory& panasonicMemory;
	const unsigned baseAddr;
};

} // namespace openmsx

#endif

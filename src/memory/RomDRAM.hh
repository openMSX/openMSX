// $Id$

#ifndef _ROMDRAM_HH_
#define _ROMDRAM_HH_

#include "MSXRom.hh"

namespace openmsx {

class PanasonicMemory;

class RomDRAM : public MSXRom
{
public:
	RomDRAM(const XMLElement& config, const EmuTime& time,
	        std::auto_ptr<Rom> rom);
	virtual ~RomDRAM();
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

private:
	PanasonicMemory& panasonicMemory;
	unsigned baseAddr;
};

} // namespace openmsx

#endif

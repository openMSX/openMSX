// $Id$

#include "RomCrossBlaim.hh"
#include "Rom.hh"
#include "CPU.hh"

namespace openmsx {

RomCrossBlaim::RomCrossBlaim(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom)
	: MSXDevice(config, time), Rom16kBBlocks(config, time, rom)
{
	reset(time);
}

RomCrossBlaim::~RomCrossBlaim()
{
}

void RomCrossBlaim::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 0);
	setBank(3, unmappedRead);
}

void RomCrossBlaim::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if (address == 0x4045) {
		setRom(2, value);
	}
}

byte* RomCrossBlaim::getWriteCacheLine(word address) const
{
	if (address == (0x4045 & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

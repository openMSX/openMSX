// $Id$

#include "RomHarryFox.hh"
#include "CPU.hh"
#include "Rom.hh"

namespace openmsx {

RomHarryFox::RomHarryFox(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom)
	: MSXDevice(config, time), Rom16kBBlocks(config, time, rom)
{
	reset(time);
}

RomHarryFox::~RomHarryFox()
{
}

void RomHarryFox::reset(const EmuTime& time)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmappedRead);
}

void RomHarryFox::writeMem(word address, byte value, const EmuTime& time)
{
	if (address == 0x6000) {
		setRom(1, 2 * (value & 1));
	}
	if (address == 0x7000) {
		setRom(2, 2 * (value & 1) + 1);
	}
}

byte* RomHarryFox::getWriteCacheLine(word address) const
{
	if ((address == (0x6000 & CPU::CACHE_LINE_HIGH)) ||
	    (address == (0x7000 & CPU::CACHE_LINE_HIGH))) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

// $Id$

#include "RomGeneric16kB.hh"

namespace openmsx {

RomGeneric16kB::RomGeneric16kB(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom)
	: MSXDevice(config, time), Rom16kBBlocks(config, time, rom)
{
	reset(time);
}

RomGeneric16kB::~RomGeneric16kB()
{
}

void RomGeneric16kB::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmappedRead);
}

void RomGeneric16kB::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	setRom(address >> 14, value);
}

byte* RomGeneric16kB::getWriteCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

// $Id$

#include "RomGeneric8kB.hh"
#include "Rom.hh"

namespace openmsx {

RomGeneric8kB::RomGeneric8kB(const XMLElement& config, const EmuTime& time,
                             std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(config, time, rom)
{
	reset(time);
}

RomGeneric8kB::~RomGeneric8kB()
{
}

void RomGeneric8kB::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, i - 2);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);
}

void RomGeneric8kB::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	setRom(address >> 13, value);
}

byte* RomGeneric8kB::getWriteCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

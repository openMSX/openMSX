// $Id$

#include "RomHalnote.hh"
#include "CPU.hh"
#include "Rom.hh"

namespace openmsx {

RomHalnote::RomHalnote(const XMLElement& config, const EmuTime& time,
                       std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(config, time, rom)
{
	reset(time);
}

RomHalnote::~RomHalnote()
{
}

void RomHalnote::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, 0);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);
}

void RomHalnote::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		if ((address & 0x1FFF) == 0x0FFF) {
			setRom(address >> 13, value);
		}
	}
}

byte* RomHalnote::getWriteCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000) &&
	    ((address & 0x1FFF) == (0x0FFF & CPU::CACHE_LINE_HIGH))) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

// $Id$

// Holy Qu'ran  cartridge
//  It is like an ASCII 8KB, but using the 5000h, 5400h, 5800h and 5C00h
//  addresses.


#include "RomHolyQuran.hh"
#include "Rom.hh"

namespace openmsx {

RomHolyQuran::RomHolyQuran(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom)
	: Rom8kBBlocks(config, time, rom)
{
	reset(time);
}

RomHolyQuran::~RomHolyQuran()
{
}

void RomHolyQuran::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, 0);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);
}

void RomHolyQuran::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	// TODO are switch addresses mirrored?
	if ((0x5000 <= address) && (address < 0x6000)) {
		byte region = ((address >> 10) & 3) + 2;
		setRom(region, value);
	}
}

byte* RomHolyQuran::getWriteCacheLine(word address) const
{
	if ((0x5000 <= address) && (address < 0x6000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

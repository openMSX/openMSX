// $Id$

// R-Type cartridges
//
// The address to change banks:
//  first  16kb: fixed at 0x0f or 0x17
//  second 16kb: 0x7000 - 0x7FFF (0x7000 and 0x7800 used)
//               bit 4 selects ROM chip,
//                if low  bit 3-0 select page
//                   high     2-0 
                   
#include "RomRType.hh"


RomRType::RomRType(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom16kBBlocks(config, time)
{
	reset(time);
}

RomRType::~RomRType()
{
}

void RomRType::reset(const EmuTime &time)
{
	setBank(0, unmappedRead);
	setRom (1, 0x17);
	setRom (2, 0);
	setBank(3, unmappedRead);
}

void RomRType::writeMem(word address, byte value, const EmuTime &time)
{
	if ((0x7000 <= address) && (address < 0x8000)) {
		value &= (value & 0x10) ? 0x17 : 0x1F;
		setRom(2, value);
	}
}

byte* RomRType::getWriteCacheLine(word address) const
{
	if ((0x7000 <= address) && (address < 0x8000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

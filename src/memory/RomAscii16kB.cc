// $Id$

// ASCII 16kB cartridges
//
// this type is used in a few cartridges.
// example of cartridges: Xevious, Fantasy Zone 2,
// Return of Ishitar, Androgynus, Gallforce ...
//
// The address to change banks:
//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)

#include "RomAscii16kB.hh"
#include "Rom.hh"

namespace openmsx {

RomAscii16kB::RomAscii16kB(const XMLElement& config, const EmuTime& time,
                           auto_ptr<Rom> rom)
	: MSXDevice(config, time), Rom16kBBlocks(config, time, rom)
{
	reset(time);
}

RomAscii16kB::~RomAscii16kB()
{
}

void RomAscii16kB::reset(const EmuTime& time)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 0);
	setBank(3, unmappedRead);
}

void RomAscii16kB::writeMem(word address, byte value, const EmuTime& time)
{
	if ((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) {
		byte region = ((address >> 12) & 1) + 1;
		setRom(region, value);
	}
}

byte* RomAscii16kB::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

// $Id$

// Korean 126-in-1 cartridge
// 
// Information obtained by studying MESS sources:
//    0x4001 : 0x4000-0x7FFF
//    0x4002 : 0x8000-0xBFFF


#include "RomKorean126in1.hh"
#include "CPU.hh"

namespace openmsx {

RomKorean126in1::RomKorean126in1(Config* config, const EmuTime& time, auto_ptr<Rom> rom)
	: MSXDevice(config, time), Rom16kBBlocks(config, time, rom)
{
	reset(time);
}

RomKorean126in1::~RomKorean126in1()
{
}

void RomKorean126in1::reset(const EmuTime& time)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmappedRead);
}

void RomKorean126in1::writeMem(word address, byte value, const EmuTime& time)
{
	if (address == 0x4000) {
		setRom(1, value);
	} else if (address == 0x4001) {
		setRom(2, value);
	}
}

byte* RomKorean126in1::getWriteCacheLine(word address) const
{
	if (address == (0x4000 & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

// $Id$

// KONAMI4 8kB cartridges
//
// this type is used by Konami cartridges that do not have an SCC and some others
// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
// The Maze of Galious, Aleste 1, 1942, Heaven, Mystery World, ...
//
// page at 4000 is fixed, other banks are switched
// by writting at 0x6000,0x8000 and 0xa000

#include "RomKonami4.hh"
#include "CPU.hh"
#include "Rom.hh"

namespace openmsx {

RomKonami4::RomKonami4(const XMLElement& config, const EmuTime& time,
                       std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(config, time, rom)
{
	reset(time);
}

RomKonami4::~RomKonami4()
{
}

void RomKonami4::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, i - 2);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);
}

void RomKonami4::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if ((address == 0x6000) ||
	    (address == 0x8000) ||
	    (address == 0xA000)) {
		setRom(address >> 13, value);
	}
}

byte* RomKonami4::getWriteCacheLine(word address) const
{
	if ((address == (0x6000 & CPU::CACHE_LINE_HIGH)) ||
	    (address == (0x8000 & CPU::CACHE_LINE_HIGH)) ||
	    (address == (0xA000 & CPU::CACHE_LINE_HIGH))) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

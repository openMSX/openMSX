// $Id$

// KONAMI4 8kB cartridges
//
// this type is used by Konami cartridges without SCC and some others
// examples of cartridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
// The Maze of Galious, Aleste 1, 1942, Heaven, Mystery World, ...
//
// page at 4000 is fixed, other banks are switched by writting at
// 0x6000, 0x8000 and 0xA000 (those addresses are used by the games, but any
// other address in a page switches that page as well)

#include "RomKonami4.hh"
#include "CPU.hh"
#include "Rom.hh"

namespace openmsx {

RomKonami4::RomKonami4(MSXMotherBoard& motherBoard, const XMLElement& config,
                       const EmuTime& time, std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(motherBoard, config, time, rom)
{
	reset(time);
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
	// Note: [0x4000..0x6000) is fixed at segment 0.
	if (0x6000 <= address && address < 0xC000) {
		setRom(address >> 13, value);
	}
}

byte* RomKonami4::getWriteCacheLine(word address) const
{
	return (0x6000 <= address && address < 0xC000) ? NULL : unmappedWrite;
}

} // namespace openmsx

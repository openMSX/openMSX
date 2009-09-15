// $Id$

// KONAMI 8kB cartridges (without SCC)
//
// this type is used by Konami cartridges without SCC and some others
// examples of cartridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
// The Maze of Galious, Aleste 1, 1942, Heaven, Mystery World, ...
// (the latter four are hacked ROM images with modified mappers)
//
// page at 4000 is fixed, other banks are switched by writting at
// 0x6000, 0x8000 and 0xA000 (those addresses are used by the games, but any
// other address in a page switches that page as well)

#include "RomKonami.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomKonami::RomKonami(MSXMotherBoard& motherBoard, const XMLElement& config,
                       std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(motherBoard, config, rom)
{
	// Konami mapper is 256kB in size, even if ROM is smaller.
	setBlockMask(31);
	// Do not call reset() here, since it can be overridden and the subclass
	// constructor has not been run yet. And there will be a reset() at power
	// up anyway.
}

void RomKonami::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	for (int i = 2; i < 6; i++) {
		setRom(i, i - 2);
	}
	setUnmapped(6);
	setUnmapped(7);
}

void RomKonami::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	// Note: [0x4000..0x6000) is fixed at segment 0.
	if (0x6000 <= address && address < 0xC000) {
		setRom(address >> 13, value);
	}
}

byte* RomKonami::getWriteCacheLine(word address) const
{
	return (0x6000 <= address && address < 0xC000) ? NULL : unmappedWrite;
}

template<typename Archive>
void RomKonami::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(RomKonami);
REGISTER_MSXDEVICE(RomKonami, "RomKonami");

} // namespace openmsx

// KONAMI 8kB cartridges (without SCC)
//
// this type is used by Konami cartridges without SCC and some others
// examples of cartridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
// The Maze of Galious, Aleste 1, 1942, Heaven, Mystery World, ...
// (the latter four are hacked ROM images with modified mappers)
//
// page at 4000 is fixed, other banks are switched by writing at
// 0x6000, 0x8000 and 0xA000 (those addresses are used by the games, but any
// other address in a page switches that page as well)

#include "RomKonami.hh"
#include "MSXMotherBoard.hh"
#include "MSXCliComm.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomKonami::RomKonami(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	// Konami mapper is 256kB in size, even if ROM is smaller.
	setBlockMask(31);

	// warn if a ROM is used that would not work on a real Konami mapper
	if (rom.size() > 256 * 1024) {
		getMotherBoard().getMSXCliComm().printWarning(
			"The size of this ROM image is larger than 256kB, "
			"which is not supported on real Konami mapper chips!");
	}

	// Do not call reset() here, since it can be overridden and the subclass
	// constructor has not been run yet. And there will be a reset() at power
	// up anyway.
}

void RomKonami::bankSwitch(unsigned page, unsigned block)
{
	setRom(page, block);

	// Note: the mirror behavior is different from RomKonamiSCC !
	if (page == 2 || page == 3) {
		// [0x4000-0x8000), mirrored in [0x0000-0x4000)
		setRom(page - 2, block);
	} else if (page == 4 || page == 5) {
		// [0x8000-0xC000), mirrored in [0xC000-0x10000)
		setRom(page + 2, block);
	} else {
		assert(false);
	}
}

void RomKonami::reset(EmuTime::param /*time*/)
{
	for (auto i : xrange(2, 6)) {
		bankSwitch(i, i - 2);
	}
}

void RomKonami::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	// Note: [0x4000..0x6000) is fixed at segment 0.
	if (0x6000 <= address && address < 0xC000) {
		bankSwitch(address >> 13, value);
	}
}

byte* RomKonami::getWriteCacheLine(word address) const
{
	return (0x6000 <= address && address < 0xC000) ? nullptr : unmappedWrite.data();
}

template<typename Archive>
void RomKonami::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(RomKonami);
REGISTER_MSXDEVICE(RomKonami, "RomKonami");

} // namespace openmsx

// Korean 128-in-1 cartridge for Sega/MSX, see
// https://www.smspower.org/forums/17902-DumpingKorean128128In1CartridgeLikelyPirateMissingOriginalLabel
//
// Reverse engineered by bsittler.
//
//    ... the unusual MSX mapper this dump normally would require, where writing
//    byte Y to address 0x6000 would map 0x4000-0x5FFF to Y, 0x6000-0x7FFF to Y
//    XOR 1, 0x8000-0x9FFF to Y XOR 2, and 0xA000-0xBFFF to Y XOR 3.
//
// Those are 8KB page numbers, naturally. And yes, it really is XOR and not
// addition - sometimes two games share a page and map it differently in a way
// that only works when the operation is XOR.


#include "RomKorean128in1.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomKorean128in1::RomKorean128in1(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomKorean128in1::reset(EmuTime /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	// TODO: initial mapping not known (?)
	for (auto i : xrange(2, 6)) {
		setRom(i, i - 2);
	}
	setUnmapped(6);
	setUnmapped(7);
}

void RomKorean128in1::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if (address == 0x6000) {
		for (auto i : xrange(2, 6)) {
			setRom(i, value ^ (i - 2));
		}
	}
}

byte* RomKorean128in1::getWriteCacheLine(uint16_t address)
{
	if (address == (0x6000 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

REGISTER_MSXDEVICE(RomKorean128in1, "RomKorean128in1");

} // namespace openmsx

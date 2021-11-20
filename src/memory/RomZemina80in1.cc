// Zemina 80-in-1 cartridge
//
// Information obtained by studying MESS sources:
//   0x4000 : 0x4000-0x5FFF
//   0x4001 : 0x6000-0x7FFF
//   0x4002 : 0x8000-0x9FFF
//   0x4003 : 0xA000-0xBFFF

#include "RomZemina80in1.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomZemina80in1::RomZemina80in1(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomZemina80in1::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	for (auto i : xrange(2, 6)) {
		setRom(i, i - 2);
	}
	setUnmapped(6);
	setUnmapped(7);
}

void RomZemina80in1::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x4000 <= address) && (address < 0x4004)) {
		setRom(2 + (address - 0x4000), value);
	}
}

byte* RomZemina80in1::getWriteCacheLine(word address) const
{
	if (address == (0x4000 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

REGISTER_MSXDEVICE(RomZemina80in1, "RomZemina80in1");

} // namespace openmsx

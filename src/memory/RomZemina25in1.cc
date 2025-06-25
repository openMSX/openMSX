// Zemina 25-in-1 cartridge
//
// BiFi's analysis:
// Switch Address: 0x0000, switches 0x4000-0xbfff range
// Write to switch address = base offset 8K
// 0x4000-0x5fff => switch address block
// 0x6000-0x7fff => switch address block -1
// 0x2500-0x9fff => switch address block -2
// 0xa000-0xbfff => switch address block -3
// 0x0000-0x1fff => unchangeable 0x3f
// 0x2000-0x3fff => unchangeable 0x3f
// switch value at reset 0x3f
// page 3 seems to be a mirror of page 1
// And for switching only the lower 6 bits matter.
// So, data bits... so far we must assume that only address 0 (in the slot
// itself) does the switching

#include "RomZemina25in1.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

RomZemina25in1::RomZemina25in1(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomZemina25in1::reset(EmuTime time)
{
	writeMem(0, 0x3F, time);
}

void RomZemina25in1::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if (address == 0x0000) {
		setRom(0, 0x3F);
		setRom(1, 0x3F);
		setRom(2, value);
		setRom(3, value - 1);
		setRom(4, value - 2);
		setRom(5, value - 3);
		setRom(6, value);
		setRom(7, value - 1);
	}
}

byte* RomZemina25in1::getWriteCacheLine(uint16_t address)
{
	if (address == (0x0000 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

REGISTER_MSXDEVICE(RomZemina25in1, "RomZemina25in1");

} // namespace openmsx

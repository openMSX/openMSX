// Japanese MSX Write (1)
// Identical to ASCII 16kB cartridges, but with 2 extra switch addresses:
// 0x6FFF and 0x7FFF

#include "RomMSXWrite.hh"
#include "CacheLine.hh"
#include "one_of.hh"
#include "serialize.hh"

namespace openmsx {

RomMSXWrite::RomMSXWrite(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomMSXWrite::reset(EmuTime /*time*/)
{
	setUnmapped(0);
	setRom(1, 0);
	setRom(2, 0);
	setUnmapped(3);
}

void RomMSXWrite::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if (((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) ||
	    (address == one_of(0x6FFF, 0x7FFF))) {
		byte region = ((address >> 12) & 1) + 1;
		setRom(region, value);
	}
}

byte* RomMSXWrite::getWriteCacheLine(uint16_t address)
{
	if (((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) ||
	    (address == one_of(0x6FFF & CacheLine::HIGH, 0x7FFF & CacheLine::HIGH))) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

REGISTER_MSXDEVICE(RomMSXWrite, "RomMSXWrite");

} // namespace openmsx

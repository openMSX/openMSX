// R-Type cartridges
//
// The address to change banks:
//  first  16kb: fixed at 0x0f or 0x17 (both have the same content)
//  second 16kb: 0x4000 - 0x7FFF (0x7000 and 0x7800 used)
//               bit 4 selects ROM chip,
//                if low  bit 3-0 select page
//                   high     2-0
// Thanks to n_n for investigating this on a real cartridge.

#include "RomRType.hh"
#include "serialize.hh"

namespace openmsx {

RomRType::RomRType(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomRType::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setRom(1, 0x17);
	setRom(2, 0);
	setUnmapped(3);
}

void RomRType::writeMem(uint16_t address, byte value, EmuTime::param /*time*/)
{
	if ((0x4000 <= address) && (address < 0x8000)) {
		value &= (value & 0x10) ? 0x17 : 0x1F;
		setRom(2, value);
	}
}

byte* RomRType::getWriteCacheLine(uint16_t address)
{
	if ((0x4000 <= address) && (address < 0x8000)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

REGISTER_MSXDEVICE(RomRType, "RomRType");

} // namespace openmsx

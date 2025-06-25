// ASCII 8kB cartridges
//
// this type is used in many japanese-only cartridges.
// example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun,
//                        Ashguine 2, ...
// The address to change banks:
//  bank 1: 0x6000 - 0x67ff (0x6000 used)
//  bank 2: 0x6800 - 0x6fff (0x6800 used)
//  bank 3: 0x7000 - 0x77ff (0x7000 used)
//  bank 4: 0x7800 - 0x7fff (0x7800 used)

#include "RomAscii8kB.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomAscii8kB::RomAscii8kB(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	RomAscii8kB::reset(EmuTime::dummy());
}

void RomAscii8kB::reset(EmuTime /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	for (auto i : xrange(2, 6)) {
		setRom(i, 0);
	}
	setUnmapped(6);
	setUnmapped(7);
}

void RomAscii8kB::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		byte region = ((address >> 11) & 3) + 2;
		setRom(region, value);
	}
}

byte* RomAscii8kB::getWriteCacheLine(uint16_t address)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

REGISTER_MSXDEVICE(RomAscii8kB, "RomAscii8kB");

} // namespace openmsx

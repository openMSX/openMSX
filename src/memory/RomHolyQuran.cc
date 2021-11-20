// Holy Qu'ran  cartridge
//  It is like an ASCII 8KB, but using the 5000h, 5400h, 5800h and 5C00h
//  addresses.

#include "RomHolyQuran.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomHolyQuran::RomHolyQuran(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomHolyQuran::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	for (auto i : xrange(2, 6)) {
		setRom(i, 0);
	}
	setUnmapped(6);
	setUnmapped(7);
}

void RomHolyQuran::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	// TODO are switch addresses mirrored?
	if ((0x5000 <= address) && (address < 0x6000)) {
		byte region = ((address >> 10) & 3) + 2;
		setRom(region, value);
	}
}

byte* RomHolyQuran::getWriteCacheLine(word address) const
{
	if ((0x5000 <= address) && (address < 0x6000)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

REGISTER_MSXDEVICE(RomHolyQuran, "RomHolyQuran");

} // namespace openmsx

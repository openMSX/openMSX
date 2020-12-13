#include "RomGeneric8kB.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomGeneric8kB::RomGeneric8kB(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomGeneric8kB::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	for (auto i : xrange(2, 6)) {
		setRom(i, i - 2);
	}
	setUnmapped(6);
	setUnmapped(7);
}

void RomGeneric8kB::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	setRom(address >> 13, value);
}

byte* RomGeneric8kB::getWriteCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

REGISTER_MSXDEVICE(RomGeneric8kB, "RomGeneric8kB");

} // namespace openmsx

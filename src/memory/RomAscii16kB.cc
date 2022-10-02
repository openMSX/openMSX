// ASCII 16kB cartridges
//
// this type is used in a few cartridges.
// example of cartridges: Xevious, Fantasy Zone 2,
// Return of Ishitar, Androgynus, Gallforce ...
//
// The address to change banks:
//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)

#include "RomAscii16kB.hh"
#include "serialize.hh"

namespace openmsx {

RomAscii16kB::RomAscii16kB(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	RomAscii16kB::reset(EmuTime::dummy());
}

void RomAscii16kB::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setRom(1, 0);
	setRom(2, 0);
	setUnmapped(3);
}

void RomAscii16kB::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) {
		byte region = ((address >> 12) & 1) + 1;
		setRom(region, value);
	}
}

byte* RomAscii16kB::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

REGISTER_MSXDEVICE(RomAscii16kB, "RomAscii16kB");

} // namespace openmsx

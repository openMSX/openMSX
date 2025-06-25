// Padial 16kB
//
// The address to change banks:
//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)

#include "RomPadial16kB.hh"
#include "serialize.hh"

namespace openmsx {

RomPadial16kB::RomPadial16kB(const DeviceConfig& config, Rom&& rom_)
	: RomAscii16kB(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomPadial16kB::reset(EmuTime /*time*/)
{
	setRom(0, 0);
	setRom(1, 0);
	setRom(2, 2);
	setUnmapped(3);
}

REGISTER_MSXDEVICE(RomPadial16kB, "RomPadial16kB");

} // namespace openmsx

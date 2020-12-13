// Padial 8kB
//
// The address to change banks:
//  bank 1: 0x6000 - 0x67ff (0x6000 used)
//  bank 2: 0x6800 - 0x6fff (0x6800 used)
//  bank 3: 0x7000 - 0x77ff (0x7000 used)
//  bank 4: 0x7800 - 0x7fff (0x7800 used)

#include "RomPadial8kB.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomPadial8kB::RomPadial8kB(const DeviceConfig& config, Rom&& rom_)
	: RomAscii8kB(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomPadial8kB::reset(EmuTime::param /*time*/)
{
	setRom(0, 0);
	setRom(1, 0);
	for (auto i : xrange(2, 6)) {
		setRom(i, i - 2);
	}
	setUnmapped(6);
	setUnmapped(7);
}

REGISTER_MSXDEVICE(RomPadial8kB, "RomPadial8kB");

} // namespace openmsx

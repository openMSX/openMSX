#include "RomNamco.hh"

#include "MSXException.hh"

namespace openmsx {

RomNamco::RomNamco(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	// Namco's MSX arcade ports (Galaga, Dig Dug, Bosconian, King &
	// Balloon, Mappy, Pac-Man, Rally-X) use a 16kB ROM chip on a
	// cartridge that ties the chip's A13 line to the MSX A15 line and
	// leaves the MSX A13/A14 lines unconnected. So MSX A15 selects which
	// 8kB half of the chip is visible, and that half is seen twice in its
	// 16kB page. The chip is only selected in pages 1 and 2:
	//   0x0000-0x3FFF -> not mapped
	//   0x4000-0x7FFF -> block 0 (mirrored at 0x6000-0x7FFF)
	//   0x8000-0xBFFF -> block 1 (mirrored at 0xA000-0xBFFF)
	//   0xC000-0xFFFF -> not mapped
	// (verified by ricbit on a real Rally-X cartridge).
	//
	// Note: dumping such a cartridge through the MSX address space yields a
	// 32kB image with each 8kB block doubled ([b0,b0,b1,b1]). Those (more
	// common) overdumps already load correctly as the plain 'Mirrored'
	// type; this mapper exists for the trimmed 16kB originals.
	if (rom.size() != 0x4000) {
		throw MSXException(rom.getName(),
			": Namco ROM must be exactly 16kB.");
	}
	setUnmapped(0);
	setUnmapped(1);
	setRom(2, 0);
	setRom(3, 0);
	setRom(4, 1);
	setRom(5, 1);
	setUnmapped(6);
	setUnmapped(7);
}

REGISTER_MSXDEVICE(RomNamco, "RomNamco");

} // namespace openmsx

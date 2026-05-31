#include "RomNamco.hh"

#include "MSXException.hh"

#include "xrange.hh"

namespace openmsx {

RomNamco::RomNamco(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	// Namco's MSX arcade ports (Galaga, Dig Dug, Bosconian, King &
	// Balloon) use a 16kB ROM chip on a cartridge that ties the chip's A13
	// line to the MSX A15 line and leaves the MSX A13/A14 lines
	// unconnected. So MSX A15 selects which 8kB half of the chip is
	// visible, and that half is mirrored across the whole 32kB region:
	//   0x0000-0x7FFF -> first  8kB block
	//   0x8000-0xFFFF -> second 8kB block
	// As a regular cartridge this is active in pages 1+2, giving:
	//   0x4000-0x7FFF -> block 0 (mirrored at 0x6000-0x7FFF)
	//   0x8000-0xBFFF -> block 1 (mirrored at 0xA000-0xBFFF)
	//
	// Note: dumping such a cartridge through the MSX address space yields a
	// 32kB image with each 8kB block doubled ([b0,b0,b1,b1]). Those (more
	// common) overdumps already load correctly as the plain 'Mirrored'
	// type; this mapper exists for the trimmed 16kB originals.
	if (rom.size() != 0x4000) {
		throw MSXException(rom.getName(),
			": Namco ROM must be exactly 16kB.");
	}
	for (auto page : xrange(8)) {
		setRom(page, (page < 4) ? 0 : 1);
	}
}

REGISTER_MSXDEVICE(RomNamco, "RomNamco");

} // namespace openmsx

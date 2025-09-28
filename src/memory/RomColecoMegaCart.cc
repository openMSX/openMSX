#include "RomColecoMegaCart.hh"

#include "CacheLine.hh"
#include "MSXException.hh"
#include "serialize.hh"

#include "one_of.hh"

// information source:
// https://github.com/openMSX/openMSX/files/2118720/MegaCart.FAQ.V1-2.pdf

namespace openmsx {

RomColecoMegaCart::RomColecoMegaCart(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	if (auto size = rom.size() / 1024;
	    size != one_of(128u, 256u, 512u, 1024u)) {
		throw MSXException(
			"MegaCart only supports ROMs of 128kB, 256kB, 512kB and 1024kB "
			"size and not of ", size, "kB.");
	}
	reset(EmuTime::dummy());
}

void RomColecoMegaCart::reset(EmuTime /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	// The first 16K of the cartridge (CV 8000h-BFFFh) is fixed and will
	// always contain the highest/last 16K segment of the EPROM.
	setRom(2, unsigned(-1)); // select last block
	setRom(3, 0);
	invalidateDeviceRCache(0xFFC0 & CacheLine::HIGH, CacheLine::SIZE);
}

byte RomColecoMegaCart::readMem(uint16_t address, EmuTime time)
{
	// The last 64 locations will switch banks (FFC0-FFFF). If you have
	// fewer than 64 banks, then the strobe addresses simply repeat where
	// they end. If you have 16 banks, then bank 0 can be strobed at FFC0,
	// FFD0, FFE0, or FFF0.
	if (address >= 0xFFC0) {
		setRom(3, address); // mirroring is handled in superclass
		invalidateDeviceRCache(0xFFC0 & CacheLine::HIGH, CacheLine::SIZE);
	}
	return Rom16kBBlocks::readMem(address, time);
}

const byte* RomColecoMegaCart::getReadCacheLine(uint16_t start) const
{
	if (start >= (0xFFC0 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return Rom16kBBlocks::getReadCacheLine(start);
	}
}

REGISTER_MSXDEVICE(RomColecoMegaCart, "RomColecoMegaCart");

} // namespace openmsx

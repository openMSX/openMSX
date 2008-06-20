// $Id$

#include "RomMSXDOS2.hh"
#include "Rom.hh"
#include "CacheLine.hh"
#include "MSXException.hh"
#include <cassert>

namespace openmsx {

RomMSXDOS2::RomMSXDOS2(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		std::auto_ptr<Rom> rom_)
	: Rom16kBBlocks(motherBoard, config, rom_)
{
	range = (*rom)[0x94];
	if ((range != 0x00) && (range != 0x60) && (range != 0x7f)) {
		throw MSXException("Invalid rom for MSXDOS2 mapper");
	}
	reset(*static_cast<EmuTime*>(0));
}

void RomMSXDOS2::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setBank(2, unmappedRead);
	setBank(3, unmappedRead);
}

void RomMSXDOS2::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	switch (range) {
	case 0x00:
		if (address != 0x7ff0) return;
		break;
	case 0x60:
		if ((address & 0xf000) != 0x6000) return;
		break;
	case 0x7f:
		if (address != 0x7ffe) return;
		break;
	default:
		assert(false);
	}
	setRom(1, value);
}

byte* RomMSXDOS2::getWriteCacheLine(word address) const
{
	switch (range) {
	case 0x00:
		if (address == (0x7ff0 & CacheLine::HIGH)) return NULL;
		break;
	case 0x60:
		if ((address & 0xf000) == 0x6000) return NULL;
		break;
	case 0x7f:
		if (address == (0x7ffe & CacheLine::HIGH)) return NULL;
		break;
	default:
		assert(false);
	}
	return unmappedWrite;
}

} // namespace openmsx

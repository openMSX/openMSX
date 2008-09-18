// $Id$

// This mapper is used for the game "Harry Fox Yki no Maoh"

/** Thanks to enen for testing this on a real cartridge:
 *
 * Writing to 0x6xxx, 0 or 1, will switch bank 0 or 2 into 0x4000-0x7fff
 * Writing to 0x7xxx, 0 or 1, will switch bank 1 or 3 into 0x8000-0xbfff
 * 0x0000-0x3fff and 0xc000-0xffff are unmapped (read as 0xff)
 */

#include "RomHarryFox.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomHarryFox::RomHarryFox(MSXMotherBoard& motherBoard, const XMLElement& config,
                         std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, rom)
{
	reset(*static_cast<EmuTime*>(0));
}

void RomHarryFox::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmappedRead);
}

void RomHarryFox::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if        ((0x6000 <= address) && (address < 0x7000)) {
		setRom(1, 2 * (value & 1) + 0);
	} else if ((0x7000 <= address) && (address < 0x8000)) {
		setRom(2, 2 * (value & 1) + 1);
	}
}

byte* RomHarryFox::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

REGISTER_MSXDEVICE(RomHarryFox, "RomHarryFox");

} // namespace openmsx

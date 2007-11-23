// $Id$

#include "RomCrossBlaim.hh"
#include "Rom.hh"
#include "CacheLine.hh"

namespace openmsx {

RomCrossBlaim::RomCrossBlaim(
	MSXMotherBoard& motherBoard, const XMLElement& config,
	const EmuTime& time, std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, rom)
{
	reset(time);
}

void RomCrossBlaim::reset(const EmuTime& /*time*/)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 0);
	setBank(3, unmappedRead);
}

void RomCrossBlaim::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if (address == 0x4045) {
		setRom(2, value);
	}
}

byte* RomCrossBlaim::getWriteCacheLine(word address) const
{
	if (address == (0x4045 & CacheLine::HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

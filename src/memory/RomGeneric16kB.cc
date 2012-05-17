// $Id$

#include "RomGeneric16kB.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomGeneric16kB::RomGeneric16kB(const DeviceConfig& config, std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(config, rom)
{
	reset(EmuTime::dummy());
}

void RomGeneric16kB::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setRom(1, 0);
	setRom(2, 1);
	setUnmapped(3);
}

void RomGeneric16kB::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	setRom(address >> 14, value);
}

byte* RomGeneric16kB::getWriteCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

REGISTER_MSXDEVICE(RomGeneric16kB, "RomGeneric16kB");

} // namespace openmsx

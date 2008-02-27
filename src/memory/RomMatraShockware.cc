// $Id$

#include "RomMatraShockware.hh"
#include "AmdFlash.hh"
#include "Rom.hh"

namespace openmsx {

RomMatraShockware::RomMatraShockware(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		const EmuTime& time, std::auto_ptr<Rom> rom_)
	: MSXRom(motherBoard, config, rom_)
	, flash(new AmdFlash(*rom, 16, 2, 0)) // don't load/save
{
	reset(time);
}

RomMatraShockware::~RomMatraShockware()
{
}

void RomMatraShockware::reset(const EmuTime& /*time*/)
{
	flash->reset();
}

byte RomMatraShockware::peek(word address, const EmuTime& /*time*/) const
{
	return flash->peek(address);
}

byte RomMatraShockware::readMem(word address, const EmuTime& /*time*/)
{
	return flash->read(address);
}

void RomMatraShockware::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	flash->write(address + 0x10000, value);
}

const byte* RomMatraShockware::getReadCacheLine(word address) const
{
	return flash->getReadCacheLine(address);
}

byte* RomMatraShockware::getWriteCacheLine(word /*address*/) const
{
	return NULL;
}

} // namespace openmsx

// $Id$

#include "RomShockware.hh"
#include "AmdFlash.hh"
#include "Rom.hh"

namespace openmsx {

RomShockware::RomShockware(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		const EmuTime& time, std::auto_ptr<Rom> rom_)
	: MSXRom(motherBoard, config, rom_)
	, flash(new AmdFlash(*rom, 16, 2, 0)) // don't load/save
{
	reset(time);
}

RomShockware::~RomShockware()
{
}

void RomShockware::reset(const EmuTime& /*time*/)
{
	flash->reset();
}

byte RomShockware::peek(word address, const EmuTime& /*time*/) const
{
	return flash->peek(address);
}

byte RomShockware::readMem(word address, const EmuTime& /*time*/)
{
	return flash->read(address);
}

void RomShockware::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	flash->write(address + 0x10000, value);
}

const byte* RomShockware::getReadCacheLine(word address) const
{
	return flash->getReadCacheLine(address);
}

byte* RomShockware::getWriteCacheLine(word /*address*/) const
{
	return NULL;
}

} // namespace openmsx

// $Id$

#include "RomPlainFlash.hh"
#include "AmdFlash.hh"
#include "Rom.hh"

namespace openmsx {

RomPlainFlash::RomPlainFlash(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		const EmuTime& time, std::auto_ptr<Rom> rom_)
	: MSXRom(motherBoard, config, rom_)
	, flash(new AmdFlash(*rom, 16, 512 / 64, 0, config))
{
	reset(time);
}

RomPlainFlash::~RomPlainFlash()
{
}

void RomPlainFlash::reset(const EmuTime& time)
{
	flash->reset();
}

byte RomPlainFlash::peek(word address, const EmuTime& time) const
{
	return flash->peek(address);
}

byte RomPlainFlash::readMem(word address, const EmuTime& time)
{
	return flash->read(address);
}

void RomPlainFlash::writeMem(word address, byte value, const EmuTime& time)
{
	flash->write(address, value);
}

const byte* RomPlainFlash::getReadCacheLine(word address) const
{
	return flash->getReadCacheLine(address);
}

byte* RomPlainFlash::getWriteCacheLine(word /*address*/) const
{
	return NULL;
}

} // namespace openmsx

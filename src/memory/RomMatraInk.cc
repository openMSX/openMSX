// $Id$

#include "RomMatraInk.hh"
#include "AmdFlash.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomMatraInk::RomMatraInk(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		std::auto_ptr<Rom> rom_)
	: MSXRom(motherBoard, config, rom_)
	, flash(new AmdFlash(*rom, 16, 2, 0)) // don't load/save
{
	reset(*static_cast<EmuTime*>(0));
}

RomMatraInk::~RomMatraInk()
{
}

void RomMatraInk::reset(const EmuTime& /*time*/)
{
	flash->reset();
}

byte RomMatraInk::peek(word address, const EmuTime& /*time*/) const
{
	return flash->peek(address);
}

byte RomMatraInk::readMem(word address, const EmuTime& /*time*/)
{
	return flash->read(address);
}

void RomMatraInk::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	flash->write(address + 0x10000, value);
}

const byte* RomMatraInk::getReadCacheLine(word address) const
{
	return flash->getReadCacheLine(address);
}

byte* RomMatraInk::getWriteCacheLine(word /*address*/) const
{
	return NULL;
}

template<typename Archive>
void RomMatraInk::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash", *flash);
}
INSTANTIATE_SERIALIZE_METHODS(RomMatraInk);

} // namespace openmsx

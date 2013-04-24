#include "RomMatraInk.hh"
#include "AmdFlash.hh"
#include "Rom.hh"
#include "serialize.hh"
#include "memory.hh"

namespace openmsx {

RomMatraInk::RomMatraInk(const DeviceConfig& config, std::unique_ptr<Rom> rom_)
	: MSXRom(config, std::move(rom_))
	, flash(make_unique<AmdFlash>(
		*rom, std::vector<unsigned>(2, 0x10000),
		0, 0x01A4, config, false)) // don't load/save
{
	reset(EmuTime::dummy());
}

RomMatraInk::~RomMatraInk()
{
}

void RomMatraInk::reset(EmuTime::param /*time*/)
{
	flash->reset();
}

byte RomMatraInk::peekMem(word address, EmuTime::param /*time*/) const
{
	return flash->peek(address);
}

byte RomMatraInk::readMem(word address, EmuTime::param /*time*/)
{
	return flash->read(address);
}

void RomMatraInk::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	flash->write(address + 0x10000, value);
}

const byte* RomMatraInk::getReadCacheLine(word address) const
{
	return flash->getReadCacheLine(address);
}

byte* RomMatraInk::getWriteCacheLine(word /*address*/) const
{
	return nullptr;
}

template<typename Archive>
void RomMatraInk::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash", *flash);
}
INSTANTIATE_SERIALIZE_METHODS(RomMatraInk);
REGISTER_MSXDEVICE(RomMatraInk, "RomMatraInk");

} // namespace openmsx

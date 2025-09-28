#include "RomMSXtra.hh"

#include "serialize.hh"

#include "xrange.hh"

namespace openmsx {

RomMSXtra::RomMSXtra(const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, ram(config, getName() + " RAM", "MSXtra RAM", 0x0800)
{
	for (auto i : xrange(0x800)) {
		ram[i] = (i & 1) ? 0x5a : 0xa5;
	}
}

byte RomMSXtra::readMem(uint16_t address, EmuTime /*time*/)
{
	if ((0x4000 <= address) && (address < 0x6000)) {
		return rom[address & 0x1fff];
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		return ram[address & 0x07ff];
	} else {
		return 0xff;
	}
}

const byte* RomMSXtra::getReadCacheLine(uint16_t address) const
{
	if ((0x4000 <= address) && (address < 0x6000)) {
		return &rom[address & 0x1fff];
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		return &ram[address & 0x07ff];
	} else {
		return unmappedRead.data();
	}
}

// default peekMem() implementation is OK

void RomMSXtra::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		ram[address & 0x07ff] = value;
	}
}

byte* RomMSXtra::getWriteCacheLine(uint16_t address)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return &ram[address & 0x07ff];
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomMSXtra::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ram", ram);
}
INSTANTIATE_SERIALIZE_METHODS(RomMSXtra);
REGISTER_MSXDEVICE(RomMSXtra, "RomMSXtra");

} // namespace openmsx

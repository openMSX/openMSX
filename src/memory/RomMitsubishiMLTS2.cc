#include "RomMitsubishiMLTS2.hh"
#include "CacheLine.hh"
#include "serialize.hh"

#include <iostream>

namespace openmsx {

RomMitsubishiMLTS2::RomMitsubishiMLTS2(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, ram(config, getName() + " RAM", "ML-TS2 RAM", 0x2000)
{
	reset(EmuTime::dummy());
}

void RomMitsubishiMLTS2::reset(EmuTime::param /*time*/)
{
	setRom(2, 0);
}

void RomMitsubishiMLTS2::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	// TODO What are these 4 registers? Are there any more?
	//      Is there any mirroring going on?
	if        (address == 0x7f00) {
		// TODO
	} else if (address == 0x7f01) {
		// TODO
	} else if (address == 0x7f02) {
		// TODO
	} else if (address == 0x7f03) {
		// TODO
	} else if (address == 0x7FC0) {
		byte bank = ((value & 0x10) >> 2) | // transform bit-pattern
		            ((value & 0x04) >> 1) | //        ...a.b.c
		            ((value & 0x01) >> 0);  //  into  00000abc
		std::cerr << "Setting MLTS2 mapper page 1 to bank " << int(bank) << '\n';
		setRom(2, bank);
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		ram[address & 0x1FFF] = value;
	}
}

byte RomMitsubishiMLTS2::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

byte RomMitsubishiMLTS2::peekMem(word address, EmuTime::param time) const
{
	// TODO What are these 4 registers? Are there any more?
	//      Is there any mirroring going on?
	//      Is the bank select register readable? (0x7fc0)
	if        (address == 0x7f00) {
		return 0xff; // TODO
	} else if (address == 0x7f01) {
		return 0xff; // TODO
	} else if (address == 0x7f02) {
		return 0xff; // TODO
	} else if (address == 0x7f03) {
		return 0xff; // TODO
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		return ram[address & 0x1FFF];
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

const byte* RomMitsubishiMLTS2::getReadCacheLine(word address) const
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if ((0x6000 <= address) && (address < 0x8000)) {
		return &ram[address & 0x1FFF];
	}
	return Rom8kBBlocks::getReadCacheLine(address);
}

byte* RomMitsubishiMLTS2::getWriteCacheLine(word address) const
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if ((0x6000 <= address) && (address < 0x8000)) {
		return const_cast<byte*>(&ram[address & 0x1FFF]);
	}
	return unmappedWrite;
}

void RomMitsubishiMLTS2::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(RomMitsubishiMLTS2);
REGISTER_MSXDEVICE(RomMitsubishiMLTS2, "RomMitsubishiMLTS2");

} // namespace openmsx

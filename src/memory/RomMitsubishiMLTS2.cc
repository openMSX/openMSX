#include "RomMitsubishiMLTS2.hh"

#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

RomMitsubishiMLTS2::RomMitsubishiMLTS2(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, ram(config, getName() + " RAM", "ML-TS2 RAM", 0x2000)
{
	reset(EmuTime::dummy());
}

void RomMitsubishiMLTS2::reset(EmuTime /*time*/)
{
	setRom(2, 0);
}

void RomMitsubishiMLTS2::writeMem(uint16_t address, byte value, EmuTime /*time*/)
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
		byte bank = value & 0b111;
		setRom(2, bank);
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		ram[address & 0x1FFF] = value;
	}
}

byte RomMitsubishiMLTS2::readMem(uint16_t address, EmuTime time)
{
	return peekMem(address, time);
}

byte RomMitsubishiMLTS2::peekMem(uint16_t address, EmuTime time) const
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

const byte* RomMitsubishiMLTS2::getReadCacheLine(uint16_t address) const
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if ((0x6000 <= address) && (address < 0x8000)) {
		return &ram[address & 0x1FFF];
	}
	return Rom8kBBlocks::getReadCacheLine(address);
}

byte* RomMitsubishiMLTS2::getWriteCacheLine(uint16_t address)
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if ((0x6000 <= address) && (address < 0x8000)) {
		return &ram[address & 0x1FFF];
	}
	return unmappedWrite.data();
}

template<typename Archive>
void RomMitsubishiMLTS2::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(RomMitsubishiMLTS2);
REGISTER_MSXDEVICE(RomMitsubishiMLTS2, "RomMitsubishiMLTS2");

} // namespace openmsx

#include "RomNeo8.hh"

#include "serialize.hh"

#include "xrange.hh"

namespace openmsx {

RomNeo8::RomNeo8(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomNeo8::reset(EmuTime /*time*/)
{
	for (auto i : xrange(6)) {
		setRom(i, 0);
		blockReg[i] = 0;
	}
	setUnmapped(6);
	setUnmapped(7);
}

void RomNeo8::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	unsigned bbb = (address >> 11) & 0b111;
	if (bbb < 2) return;
	unsigned region = bbb - 2;
	if ((address & 1) == 0) {
		blockReg[region] = uint16_t((blockReg[region] & 0xFF00) | value);
	} else {
		blockReg[region] = uint16_t((blockReg[region] & 0x00FF) | ((value & 0b1111) << 8));
	}
	setRom(region, blockReg[region]);
}

byte* RomNeo8::getWriteCacheLine(uint16_t address)
{
	unsigned bbb = (address >> 11) & 0b111;
	return (bbb < 2) ? unmappedWrite.data() : nullptr;
}

template<typename Archive>
void RomNeo8::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("blockReg", blockReg);
}
INSTANTIATE_SERIALIZE_METHODS(RomNeo8);
REGISTER_MSXDEVICE(RomNeo8, "RomNeo8");

} // namespace openmsx

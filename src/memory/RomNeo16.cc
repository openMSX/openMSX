#include "RomNeo16.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomNeo16::RomNeo16(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomNeo16::reset(EmuTime /*time*/)
{
	for (auto i : xrange(3)) {
		setRom(i, 0);
		blockReg[i] = 0;
	}
	setUnmapped(3);
}

void RomNeo16::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	unsigned bbb = (address >> 11) & 0b111;
	if ((bbb < 2) || (bbb & 1)) return;
	unsigned region = (bbb >> 1) - 1;
	if ((address & 1) == 0) {
		blockReg[region] = uint16_t((blockReg[region] & 0xFF00) | value);
	} else {
		blockReg[region] = uint16_t((blockReg[region] & 0x00FF) | ((value & 0b1111) << 8));
	}
	setRom(region, blockReg[region]);
}

byte* RomNeo16::getWriteCacheLine(uint16_t address)
{
	unsigned bbb = (address >> 11) & 0b111;
	return ((bbb < 2) || (bbb & 1)) ? unmappedWrite.data() : nullptr;
}

template<typename Archive>
void RomNeo16::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("blockReg", blockReg);
}
REGISTER_MSXDEVICE(RomNeo16, "RomNeo16");

} // namespace openmsx

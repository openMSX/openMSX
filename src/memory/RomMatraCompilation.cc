#include "RomMatraCompilation.hh"
#include "serialize.hh"

// This is basically a generic 8kB mapper (Konami like, but without fixed page
// at 0x4000), with an extra offset register accessible at 0xBA00.
// The software only writes to 0x5000, 0x6000, 0x8000, 0xA000 to switch mapper
// page. 
// It seems that a write of 1 to the offset register should be igored and that
// the value written there (if different than 1) must be lowered by 2 to get
// the actual offset to use.

namespace openmsx {

RomMatraCompilation::RomMatraCompilation(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomMatraCompilation::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	for (int i = 2; i < 6; i++) {
		setRom(i, i - 2);
	}
	setUnmapped(6);
	setUnmapped(7);

	blockOffset = 0;
}

void RomMatraCompilation::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if (address == 0xBA00) {
		if (value >= 2) {
			// write of block offset
			blockOffset = value - 2;
			// retro-actively select the blocks for this offset
			for (int i = 2; i < 6; i++) {
				setRom(i, blockNr[i] + blockOffset);
			}
		}
	}
	else if ((0x4000 <= address) && (address < 0xC000)) {
		setRom(address >> 13, value + blockOffset);
	}
}

byte* RomMatraCompilation::getWriteCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void RomMatraCompilation::serialize(Archive& ar, unsigned /*version*/)
{
        ar.template serializeBase<Rom8kBBlocks>(*this);

	ar.serialize("blockOffset", blockOffset);
}
INSTANTIATE_SERIALIZE_METHODS(RomMatraCompilation);
REGISTER_MSXDEVICE(RomMatraCompilation, "RomMatraCompilation");

} // namespace openmsx

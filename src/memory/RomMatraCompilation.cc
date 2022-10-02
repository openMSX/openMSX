#include "RomMatraCompilation.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "xrange.hh"

// This is basically a generic 8kB mapper (Konami like, but without fixed page
// at 0x4000), with an extra offset register accessible at 0xBA00.
// The software only writes to 0x5000, 0x6000, 0x8000, 0xA000 to switch mapper
// page. I took these as unique switching addresses, as some of the compilation
// software would write to others, causing crashes.
// It seems that a write of 1 (and probably 0, but the software doesn't do
// that) to the offset register should be ignored and will disable the mapper
// switch addresses. Values of 2 or larger written there must be lowered by 2
// to get the actual offset to use.

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
	for (auto i : xrange(2, 6)) {
		setRom(i, i - 2);
	}
	setUnmapped(6);
	setUnmapped(7);

	blockOffset = 2;
}

void RomMatraCompilation::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if (address == 0xBA00) {
		// write of block offset
		blockOffset = value;
		// retro-actively select the blocks for this offset
		if (blockOffset >= 2) {
			for (auto i : xrange(2, 6)) {
				setRom(i, blockNr[i] + blockOffset - 2);
			}
		}
	} else if ((blockOffset >= 2) &&
		   (address == one_of(0x5000, 0x6000, 0x8000, 0xA000))) {
		setRom(address >> 13, value + blockOffset - 2);
	}
}

byte* RomMatraCompilation::getWriteCacheLine(word address) const
{
	if ((0x5000 <= address) && (address < 0xC000)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
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

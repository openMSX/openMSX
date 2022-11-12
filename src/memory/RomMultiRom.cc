//
// Manuel Pazos MultiROM Collection
//

#include "RomMultiRom.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomMultiRom::RomMultiRom(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
}

void RomMultiRom::reset(EmuTime::param /*time*/)
{
	++counter &= 7;
	for (auto i : xrange(4)) {
		setRom(i, counter * 4 + i);
	}
}

template<typename Archive>
void RomMultiRom::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("counter", counter);
}
INSTANTIATE_SERIALIZE_METHODS(RomMultiRom);
REGISTER_MSXDEVICE(RomMultiRom, "RomMultiRom");

} // namespace openmsx

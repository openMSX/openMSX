//

//
// Manuel Pazos MultiROM Collection
//

#include "RomMultiRom.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomMultiRom::RomMultiRom(const DeviceConfig& config, std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(config, rom)
{
	counter = 0;
	for (int i=0; i<4; i++) setRom(i, counter * 4 + i);
}

RomMultiRom::~RomMultiRom()
{
}

void RomMultiRom::reset(EmuTime::param /*time*/)
{
	++counter &= 7;
	for (int i=0; i<4; i++) setRom(i, counter * 4 + i);
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

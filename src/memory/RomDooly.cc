// $Id$

//
// Agigongnyong Dooly
//

#include "RomDooly.hh"
#include "CacheLine.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomDooly::RomDooly(MSXMotherBoard& motherBoard, const XMLElement& config,
                   std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, rom)
{
	conversion = 0;
}

RomDooly::~RomDooly()
{
}

void RomDooly::reset(EmuTime::param /*time*/)
{
	conversion = 0;
}

byte RomDooly::peekMem(word address, EmuTime::param /*time*/) const
{
	if ((0x4000 <= address) && (address < 0xc000)) {
		byte value = (*rom)[address - 0x4000];
		return (conversion == 4)
		     ? (value & 0xf8) | ((value >> 2) & 0x01) | ((value << 1) & 0x06)
		     : value;
	} else {
		return 0xff;
	}
}

byte RomDooly::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

void RomDooly::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((address & 0xf000) == 0x8000) {
		conversion = value & 0x07;
	}
}

byte* RomDooly::getWriteCacheLine(word address) const
{
	if ((address & 0xf000) == (0x8000 & CacheLine::HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void RomDooly::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("conversion", conversion);
}
INSTANTIATE_SERIALIZE_METHODS(RomDooly);
REGISTER_MSXDEVICE(RomDooly, "RomDooly");

} // namespace openmsx

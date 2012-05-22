// $Id$

//
// Agigongnyong Dooly
//

#include "RomDooly.hh"
#include "RomBlockDebuggable.hh"
#include "CacheLine.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomDooly::RomDooly(const DeviceConfig& config, std::auto_ptr<Rom> rom)
	: MSXRom(config, rom)
	, romBlockDebug(new RomBlockDebuggable(*this, &conversion, 0x4000, 0x8000, 15))
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
		switch (conversion) {
		case 0:
			return value;
		case 1:
			return (value & 0xf8) | (value << 2 & 0x04) | (value >> 1 & 0x03);
		case 4:
			return (value & 0xf8) | (value >> 2 & 0x01) | (value << 1 & 0x06);
		case 2:
		case 5:
		case 6:
			switch (value & 0x07) {
			case 1:
			case 2:
			case 4:
				return value & 0xf8;
			case 3:
			case 5:
			case 6:
				if (conversion == 2) return (value & 0xf8) | (((value << 2 & 0x04) | (value >> 1 & 0x03)) ^ 0x07);
				if (conversion == 5) return value ^ 0x07;
				if (conversion == 6) return (value & 0xf8) | (((value >> 2 & 0x01) | (value << 1 & 0x06)) ^ 0x07);
			default:
				return value;
			}
		default:
			return value | 0x07;
		}
	}
	return 0xff;
}

byte RomDooly::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

void RomDooly::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x4000 <= address) && (address < 0xc000)) {
		conversion = value & 0x07;
	}
}

byte* RomDooly::getWriteCacheLine(word address) const
{
	if (((0x4000 & CacheLine::HIGH) <= address) && (address < (0xc000 & CacheLine::HIGH))) {
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

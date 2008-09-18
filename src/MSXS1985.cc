// $Id$

#include "MSXS1985.hh"
#include "Ram.hh"
#include "serialize.hh"

namespace openmsx {

static const byte ID = 0xFE;

MSXS1985::MSXS1985(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, MSXSwitchedDevice(motherBoard, ID)
	, ram(new Ram(motherBoard, getName() + " RAM", "S1985 RAM", 0x10))
{
	// TODO load ram
}

MSXS1985::~MSXS1985()
{
	// TODO save ram
}

void MSXS1985::reset(const EmuTime& /*time*/)
{
}

byte MSXS1985::readSwitchedIO(word port, const EmuTime& time)
{
	byte result = peekSwitchedIO(port, time);
	switch (port & 0x0F) {
	case 7:
		pattern = (pattern << 1) | (pattern >> 7);
		break;
	}
	//PRT_DEBUG("S1985: read " << (int) port << " " << (int)result);
	return result;
}

byte MSXS1985::peekSwitchedIO(word port, const EmuTime& /*time*/) const
{
	byte result;
	switch (port & 0x0F) {
	case 0:
		result = byte(~ID);
		break;
	case 2:
		result = (*ram)[address];
		break;
	case 7:
		result = (pattern & 0x80) ? color2 : color1;
		break;
	default:
		result = 0xFF;
	}
	return result;
}

void MSXS1985::writeSwitchedIO(word port, byte value, const EmuTime& /*time*/)
{
	//PRT_DEBUG("S1985: write " << (int) port << " " << (int)value);
	switch (port & 0x0F) {
	case 1:
		address = value & 0x0F;
		break;
	case 2:
		(*ram)[address] = value;
		break;
	case 6:
		color2 = color1;
		color1 = value;
		break;
	case 7:
		pattern = value;
		break;
	}
}

template<typename Archive>
void MSXS1985::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// no need to serialize MSXSwitchedDevice base class

	ar.serialize("ram", *ram);
	ar.serialize("address", address);
	ar.serialize("color1", color1);
	ar.serialize("color2", color2);
	ar.serialize("pattern", pattern);
}
INSTANTIATE_SERIALIZE_METHODS(MSXS1985);
REGISTER_MSXDEVICE(MSXS1985, "S1985");

} // namespace openmsx

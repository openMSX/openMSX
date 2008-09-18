// $Id$

#include "MC6850.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

MC6850::MC6850(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
}

void MC6850::reset(const EmuTime& /*time*/)
{
}

byte MC6850::readIO(word port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MC6850::peekIO(word port, const EmuTime& /*time*/) const
{
	byte result;
	switch (port & 0x1) {
	case 0: // read status
		result = 2;
		break;
	case 1: // read data
		result = 0;
		break;
	default: // unreachable, avoid warning
		assert(false);
		result = 0;
	}
	//PRT_DEBUG("Audio: read "<<hex<<(int)port<<" "<<(int)result<<dec);
	return result;
}

void MC6850::writeIO(word port, byte /*value*/, const EmuTime& /*time*/)
{
	switch (port & 0x01) {
	case 0: // control
		break;
	case 1: // write data
		break;
	}
}

template<typename Archive>
void MC6850::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(MC6850);
REGISTER_MSXDEVICE(MC6850, "MC6850");

} // namespace openmsx

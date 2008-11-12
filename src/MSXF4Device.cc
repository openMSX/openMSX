// $Id$

#include "MSXF4Device.hh"
#include "XMLElement.hh"
#include "serialize.hh"

namespace openmsx {

MSXF4Device::MSXF4Device(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, inverted(config.getChildDataAsBool("inverted", false))
{
	reset(*static_cast<EmuTime*>(0));
}

void MSXF4Device::reset(EmuTime::param /*time*/)
{
	status = inverted ? 0xFF : 0x00;
}

byte MSXF4Device::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXF4Device::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	return status;
}

void MSXF4Device::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	if (inverted) {
		status = value | 0x7F;
	} else {
		status = (status & 0x20) | (value & 0xA0);
	}
}

template<typename Archive>
void MSXF4Device::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("status", status);
}
INSTANTIATE_SERIALIZE_METHODS(MSXF4Device);
REGISTER_MSXDEVICE(MSXF4Device, "F4Device");

} // namespace openmsx

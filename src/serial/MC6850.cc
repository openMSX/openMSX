#include "MC6850.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

MC6850::MC6850(const DeviceConfig& config)
	: MSXDevice(config)
{
}

void MC6850::reset(EmuTime::param /*time*/)
{
}

byte MC6850::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MC6850::peekIO(word port, EmuTime::param /*time*/) const
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
		UNREACHABLE; result = 0;
	}
	return result;
}

void MC6850::writeIO(word port, byte /*value*/, EmuTime::param /*time*/)
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

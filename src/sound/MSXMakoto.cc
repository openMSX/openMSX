#include "MSXMakoto.hh"

namespace openmsx {

MSXMakoto::MSXMakoto(DeviceConfig& config)
	: MSXDevice(config)
	, ym2608(config, getName(), getCurrentTime())
{
}

void MSXMakoto::reset(EmuTime time)
{
	ym2608.reset(time);
}

uint8_t MSXMakoto::readIO(uint16_t port, EmuTime time)
{
	return ym2608.read(port & 3, time);
}

uint8_t MSXMakoto::peekIO(uint16_t port, EmuTime time) const
{
	return ym2608.peek(port & 3, time);
}

void MSXMakoto::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	ym2608.write(port & 3, value, time);
}

template<typename Archive>
void MSXMakoto::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ym2608", ym2608);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMakoto);
REGISTER_MSXDEVICE(MSXMakoto, "Makoto");

} // namespace openmsx

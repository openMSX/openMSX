#include "SNPSG.hh"

#include "serialize.hh"

namespace openmsx {

SNPSG::SNPSG(const DeviceConfig& config)
	: MSXDevice(config)
	, sn76489(config)
{
}

void SNPSG::reset(EmuTime time)
{
	sn76489.reset(time);
}

void SNPSG::writeIO(uint16_t /*port*/, byte value, EmuTime time)
{
	// The chip has only a single port.
	sn76489.write(value, time);
}

template<typename Archive>
void SNPSG::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("sn76489", sn76489);
}
INSTANTIATE_SERIALIZE_METHODS(SNPSG);
REGISTER_MSXDEVICE(SNPSG, "SN76489 PSG");

} // namespace openmsx

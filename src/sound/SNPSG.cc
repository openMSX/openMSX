#include "SNPSG.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

SNPSG::SNPSG(const DeviceConfig& config)
	: MSXDevice(config)
	, sn76489(config)
{
}

void SNPSG::reset(EmuTime::param time)
{
	sn76489.reset(time);
}

void SNPSG::writeIO(word /*port*/, byte value, EmuTime::param time)
{
	// The chip has only a single port.
	sn76489.write(value, time);
}

void SNPSG::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("sn76489", sn76489);
}
INSTANTIATE_SERIALIZE_METHODS(SNPSG);
REGISTER_MSXDEVICE(SNPSG, "SN76489 PSG");

} // namespace openmsx

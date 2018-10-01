#include "DACSound8U.hh"

namespace openmsx {

DACSound8U::DACSound8U(string_view name_, string_view desc,
                       const DeviceConfig& config)
	: DACSound16S(name_, desc, config)
{
}

void DACSound8U::writeDAC(uint8_t value, EmuTime::param time)
{
	DACSound16S::writeDAC((int16_t(value) - 0x80) << 8, time);
}

} // namespace openmsx

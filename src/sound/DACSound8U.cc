#include "DACSound8U.hh"
#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

DACSound8U::DACSound8U(std::string_view name_, static_string_view desc,
                       const DeviceConfig& config)
	: DACSound16S(name_, desc, config)
{
	// Apply 8->16 bit scaling as part of volume-multiplication (= for
	// free) instead of on each writeDAC() invocation.
	setSoftwareVolume(256.0f, config.getMotherBoard().getCurrentTime());
}

void DACSound8U::writeDAC(uint8_t value, EmuTime::param time)
{
	DACSound16S::writeDAC(int(value) - 0x80, time);
}

} // namespace openmsx

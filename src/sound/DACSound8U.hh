// This class implements a 8 bit unsigned DAC

#ifndef DACSOUND8U_HH
#define DACSOUND8U_HH

#include "DACSound16S.hh"

namespace openmsx {

class DACSound8U final : public DACSound16S
{
public:
	DACSound8U(std::string_view name, static_string_view desc,
	           const DeviceConfig& config);

	void writeDAC(uint8_t value, EmuTime time);
};

} // namespace openmsx

#endif

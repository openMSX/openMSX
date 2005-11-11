// $Id$

#include "DACSound8U.hh"

namespace openmsx {

DACSound8U::DACSound8U(Mixer& mixer, const std::string& name,
                       const std::string& desc, const XMLElement& config,
                       const EmuTime& time)
	: DACSound16S(mixer, name, desc, config, time)
{
}

void DACSound8U::writeDAC(byte value, const EmuTime& time)
{
	DACSound16S::writeDAC(((short)value - 0x80) << 8, time);
}

} // namespace openmsx

// $Id$

#include "DACSound8U.hh"


namespace openmsx {

DACSound8U::DACSound8U(const string& name, const string& desc,
                       const XMLElement& config, const EmuTime& time)
	: DACSound16S(name, desc, config, time)
{
}

DACSound8U::~DACSound8U()
{
}

void DACSound8U::writeDAC(byte value, const EmuTime& time)
{
	DACSound16S::writeDAC(((short)value - 0x80) << 8, time);
}

} // namespace openmsx

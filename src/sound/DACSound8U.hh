// $Id$

// This class implements a 8 bit unsigned DAC

#ifndef DACSOUND8U_HH
#define DACSOUND8U_HH

#include "DACSound16S.hh"

namespace openmsx {

class DACSound8U : public DACSound16S
{
public:
	DACSound8U(string_ref name, string_ref desc,
	           const DeviceConfig& config);

	void writeDAC(byte value, EmuTime::param time);
};

} // namespace openmsx

#endif

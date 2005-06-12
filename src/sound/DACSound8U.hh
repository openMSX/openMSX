// $Id$

// This class implements a 8 bit unsigned DAC

#ifndef DACSOUND8U_HH
#define DACSOUND8U_HH

#include "DACSound16S.hh"

namespace openmsx {

class DACSound8U : public DACSound16S
{
public:
	DACSound8U(const std::string &name, const std::string& desc,
	           const XMLElement& config, const EmuTime& time);
	virtual ~DACSound8U();

	void writeDAC(byte value, const EmuTime& time);
};

} // namespace openmsx

#endif

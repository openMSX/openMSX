// $Id$

// This class implements a 8 bit unsigned DAC

#ifndef __DACSOUND8U_HH__
#define __DACSOUND8U_HH__

#include "DACSound16S.hh"


namespace openmsx {

class DACSound8U : public DACSound16S
{
	public:
		DACSound8U(const string &name, short maxVolume,
		           const EmuTime &time); 
		virtual ~DACSound8U();
	
		void writeDAC(byte value, const EmuTime &time);
};

} // namespace openmsx

#endif

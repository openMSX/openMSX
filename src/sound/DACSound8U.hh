// $Id$

// This class implements a 8 bit unsigned DAC

#ifndef __DACSOUND8U_HH__
#define __DACSOUND8U_HH__

#include "DACSound16S.hh"


class DACSound8U : public DACSound16S
{
	public:
		DACSound8U(short maxVolume, const EmuTime &time); 
		virtual ~DACSound8U();
	
		void writeDAC(byte value, const EmuTime &time);
};

#endif

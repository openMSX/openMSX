// $Id$

#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include "MSXMemDevice.hh"
#include "MSXRomDevice.hh"


class MSXFDC : public MSXMemDevice, public MSXRomDevice
{
	public:
		MSXFDC(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXFDC();

		virtual byte readMem(word address, const EmuTime &time);
		virtual byte* getReadCacheLine(word start);
};
#endif

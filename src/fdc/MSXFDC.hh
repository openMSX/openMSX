// $Id$

#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include "MSXMemDevice.hh"
#include "MSXRomDevice.hh"

class DiskDrive;


class MSXFDC : public MSXMemDevice
{
	public:
		MSXFDC(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXFDC();

		virtual byte readMem(word address, const EmuTime &time) const;
		virtual const byte* getReadCacheLine(word start) const;
	
	protected:
		MSXRomDevice rom;
		DiskDrive* drives[4];
};
#endif

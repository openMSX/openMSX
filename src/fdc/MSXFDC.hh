// $Id$

#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include "MSXMemDevice.hh"
#include "Rom.hh"

class DiskDrive;


class MSXFDC : public MSXMemDevice
{
	public:
		MSXFDC(Device *config, const EmuTime &time);
		virtual ~MSXFDC();

		virtual byte readMem(word address, const EmuTime &time) const;
		virtual const byte* getReadCacheLine(word start) const;
	
	protected:
		Rom rom;
		DiskDrive* drives[4];
};
#endif

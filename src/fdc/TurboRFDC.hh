// $Id$

#ifndef __TURBORFDC_HH__
#define __TURBORFDC_HH__

#include "MSXFDC.hh"

class TC8566AF;


class TurboRFDC : public MSXFDC
{
	public:
		TurboRFDC(MSXConfig::Device *config, const EmuTime &time);
		virtual ~TurboRFDC();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual const byte* getReadCacheLine(word start) const;

	private:
		byte* emptyRom;
		byte* memory;
		TC8566AF* controller;
};
#endif

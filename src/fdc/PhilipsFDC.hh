// $Id$

#ifndef __PHILIPSFDC_HH__
#define __PHILIPSFDC_HH__

#include "WD2793BasedFDC.hh"


class PhilipsFDC : public WD2793BasedFDC
{
	public:
		PhilipsFDC(MSXConfig::Device *config, const EmuTime &time);
		virtual ~PhilipsFDC();
		
		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual const byte* getReadCacheLine(word start) const;

	private:
		byte* emptyRom;
		bool brokenFDCread;
		byte sideReg;
		byte driveReg;
};
#endif

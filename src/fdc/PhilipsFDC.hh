// $Id$

#ifndef __PHILIPSFDC_HH__
#define __PHILIPSFDC_HH__

#include "MSXFDC.hh"

class WD2793;


class PhilipsFDC : public MSXFDC
{
	public:
		PhilipsFDC(MSXConfig::Device *config, const EmuTime &time);
		virtual ~PhilipsFDC();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual byte* getReadCacheLine(word start);

	private:
		byte* emptyRom;
		WD2793* controller;
		bool brokenFDCread;
		byte driveReg;
};
#endif

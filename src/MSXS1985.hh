// $Id$

/*
 * This class implements the
 *   backup RAM
 *   bitmap function
 * of the S1985 MSX-engine
 * 
 *  TODO explanation  
 */

#ifndef __S1985_HH__
#define __S1985_HH__

#include "MSXIODevice.hh"


class MSXS1985 : public MSXIODevice
{
	public:
		MSXS1985(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXS1985();
		
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
	
	private:
		nibble address;
		byte ram[0x10];
	
		byte color1;
		byte color2;
		byte pattern;
};

#endif

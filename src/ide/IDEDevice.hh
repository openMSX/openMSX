// $Id$

#ifndef __IDEDevice_HH__
#define __IDEDevice_HH__

#include "openmsx.hh"

class EmuTime;


class IDEDevice
{
	public:
		virtual void reset(const EmuTime &time) = 0;
	
		virtual word readData(const EmuTime &time) = 0;
		virtual byte readReg(nibble reg, const EmuTime &time) = 0;

		virtual void writeData(word value, const EmuTime &time) = 0;
		virtual void writeReg(nibble reg, byte value, const EmuTime &time) = 0;
};
#endif

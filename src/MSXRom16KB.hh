// $Id$

#ifndef __MSXROM16KB_HH__
#define __MSXROM16KB_HH__

#include "MSXDevice.hh"
#include "emutime.hh"

class MSXRom16KB : public MSXDevice
{
	public:
		//constructor and destructor
		MSXRom16KB();
		~MSXRom16KB();
		
		// don't forget you inherited from MSXDevice
		void init();
		
		byte readMem(word address, Emutime &time);

	private:
		static const int ROM_SIZE = 16384;
		byte* memoryBank;
};
#endif

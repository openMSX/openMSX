// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include <iostream>
#include <fstream>
#include <string>
#include "EmuTime.hh"
#include "MSXDevice.hh"

class MSXMemoryMapper : public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXMemoryMapper(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXMemoryMapper();
		
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);
		
		void init();
		void reset();
	
	private:
		int getAdr(int address);

		byte *buffer;
		int blocks;
		bool slowDrainOnReset;
};

#endif //__MSXMEMORYMAPPER_HH__


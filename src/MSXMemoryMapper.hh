// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include <iostream>
#include <fstream>
#include <string>
#include "EmuTime.hh"
#include "MSXMemDevice.hh"

class MSXMemoryMapper : public MSXMemDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXMemoryMapper(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXMemoryMapper();
		
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);
		
		void reset(const EmuTime &time);
	
	private:
		int getAdr(int address);

		byte *buffer;
		int blocks;
		bool slowDrainOnReset;
};

#endif //__MSXMEMORYMAPPER_HH__


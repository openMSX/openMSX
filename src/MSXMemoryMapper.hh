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
		
		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);
		
		void reset(const EmuTime &time);
	
	private:
		word getAdr(word address);

		byte *buffer;
		int blocks;
		bool slowDrainOnReset;
};

#endif //__MSXMEMORYMAPPER_HH__


// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include <iostream>
#include <fstream>
#include <string>
#include "emutime.hh"
#include "MSXDevice.hh"

class MSXMemoryMapper : public MSXDevice
{
	public:
		MSXMemoryMapper();
		~MSXMemoryMapper();
		
		byte readMem(word address, Emutime &time);
		void writeMem(word address, byte value, Emutime &time);
		
		void init();
		void reset();
		
		//void saveState(std::ofstream &writestream);
		//void restoreState(std::string &devicestring, std::ifstream &readstream);
	
	private:
		int getAdr(int address);

		byte *buffer;
		int blocks;
		bool slow_drain_on_reset;
};

#endif //__MSXMEMORYMAPPER_HH__


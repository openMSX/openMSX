// $Id$

#ifndef __PANASONICMEMORY_HH__
#define __PANASONICMEMORY_HH__

#include "openmsx.hh"


class PanasonicMemory
{
	public:
		static PanasonicMemory* instance();
	
		void registerRom(const byte* rom, int romSize);
		void registerRam(byte* ram, int ramSize);
		const byte* getRomBlock(int block);
		byte* getRamBlock(int block);
		void setDRAM(bool dram);
	
	private:
		PanasonicMemory();
		~PanasonicMemory();
	
		const byte* rom;
		int romSize;
		byte* ram;
		int ramSize;
		bool dram;
};

#endif

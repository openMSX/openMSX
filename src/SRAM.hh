// $Id$

#ifndef __SRAM_HH__
#define __SRAM_HH__

#include "MSXConfig.hh"


class SRAM
{
	public:
		SRAM(int size, MSXConfig::Device *config,
		     const char* header = NULL);
		virtual ~SRAM();

		byte read(int address)
		{
			assert(address < size);
			return sram[address];
		}
		void write(int address, byte value)
		{
			assert(address < size);
			sram[address] = value;
		}
		byte* getBlock(int address = 0)
		{
			assert(address < size);
			return &sram[address];
		}
		int getSize()
		{
			return size;
		}
		
	private:
		int size;
		MSXConfig::Device *config;
		const char* header;
		byte* sram;
};

#endif

// $Id$

#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include "MSXMemDevice.hh"


class MSXRam : public MSXMemDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXRam(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXRam();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual byte* getReadCacheLine(word start);
		virtual byte* getWriteCacheLine(word start);

	private:
		inline bool isInside(word address);
	
		int base;
		int end;
		bool slowDrainOnReset;
		byte* memoryBank;
		byte* emptyRead;
		byte* emptyWrite;
};
#endif

// $Id$

#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include "MSXMemDevice.hh"


class MSXSimple64KB : public MSXMemDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXSimple64KB(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXSimple64KB();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual byte* getReadCacheLine(word start);
		virtual byte* getWriteCacheLine(word start);

	private:
		byte* memoryBank;
		bool slowDrainOnReset;
};
#endif

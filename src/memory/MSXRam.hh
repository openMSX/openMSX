// $Id$

#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include "MSXMemDevice.hh"


namespace openmsx {

class MSXRam : public MSXMemDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXRam(Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXRam();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual const byte* getReadCacheLine(word start) const;
		virtual byte* getWriteCacheLine(word start) const;

	private:
		inline bool isInside(word address) const;
	
		int base;
		int end;
		bool slowDrainOnReset;
		byte* memoryBank;
};

} // namespace openmsx
#endif

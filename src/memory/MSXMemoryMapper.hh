// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include "MSXMemDevice.hh"

class MSXMapperIO;


class MSXMemoryMapper : public MSXMemDevice
{
	public:
		MSXMemoryMapper(Device *config, const EmuTime &time);
		virtual ~MSXMemoryMapper();
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;
		virtual byte* getWriteCacheLine(word start) const;
		
		virtual void reset(const EmuTime &time);
	
	protected:
		/** Converts a Z80 address to a RAM address.
		  * @param address Index in Z80 address space.
		  * @return Index in RAM address space.
		  */
		inline int calcAddress(word address) const;

		byte *buffer;
		MSXMapperIO* mapperIO;
		int nbBlocks;
		bool slowDrainOnReset;
};

#endif //__MSXMEMORYMAPPER_HH__

// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include "MSXMemDevice.hh"
#include "MSXIODevice.hh"

// forward declaration;
class EmuTime;
class MSXMapperIO;


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
		byte* getReadCacheLine(word start);
		byte* getWriteCacheLine(word start);
		
		void reset(const EmuTime &time);
	
	private:
		/** Converts a Z80 address to a RAM address.
		  * @param address Index in Z80 address space.
		  * @return Index in RAM address space.
		  */
		inline int calcAddress(word address);

		byte *buffer;
		MSXMapperIO* mapperIO;
		int nbBlocks;
		bool slowDrainOnReset;
};

#endif //__MSXMEMORYMAPPER_HH__

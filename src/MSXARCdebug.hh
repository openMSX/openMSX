// $Id$

#ifndef __MSXARCdebug_HH__
#define __MSXARCdebug_HH__

#include "MSXMemDevice.hh"

// forward declarations
class EmuTime;


class MSXARCdebug : public MSXMemDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXARCdebug(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXARCdebug();
		
		void reset(const EmuTime &time);
		
		//void SaveStateMSX(ofstream savestream);
		
		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);  
		//byte* getReadCacheLine(word start);
		//byte* getWriteCacheLine(word start);

};
#endif

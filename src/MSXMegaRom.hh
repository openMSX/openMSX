// $Id$

#ifndef __MSXMEGAROM_HH__
#define __MSXMEGAROM_HH__

#include "MSXDevice.hh"
#include "emutime.hh"

class MSXMegaRom : public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXMegaRom(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXMegaRom();
		
		// don't forget you inherited from MSXDevice
		void init();
		
		byte readMem(word address, Emutime &time);
		void writeMem(word address, byte value, Emutime &time);

	private:
		int ROM_SIZE;
		byte* memoryBank;
		int mapperType;
		byte mapperMask;
		byte internalMapper[4];
      		byte *internalMemoryBank[4]; // 4 blocks of 8kB starting at #4000
		bool enabledSCC;

		int retriefMapperType();
		//MSXSCC *cartridgeSCC; //TODO write an SCC :-)

};
#endif

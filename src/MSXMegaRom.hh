// $Id$

#ifndef __MSXMEGAROM_HH__
#define __MSXMEGAROM_HH__

#include "MSXDevice.hh"
#include "EmuTime.hh"

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
		
		void init();
		
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);

	private:
		int romSize;
		byte* memoryBank;
		int mapperType;
		byte mapperMask;
		byte *internalMemoryBank[8]; // 4 blocks of 8kB starting at #4000
		bool enabledSCC;

		int retriefMapperType();
		//MSXSCC *cartridgeSCC; //TODO write an SCC :-)

};
#endif

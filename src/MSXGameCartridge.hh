// $Id$

#ifndef __MSXGAMECARTRIDGE_HH__
#define __MSXGAMECARTRIDGE_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXRom.hh"
#include "MSXMemDevice.hh"

// forward declarations
#ifndef DONT_WANT_SCC
class SCC;
#endif
class DACSound;
class EmuTime;


class MSXGameCartridge : public MSXMemDevice, public MSXRom
{
	public:
		/**
		 * Constructor
		 */
		MSXGameCartridge(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXGameCartridge();

		void reset(const EmuTime &time);

		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);
		byte* getReadCacheLine(word start);
		static const byte adr2pag[]={128,128,1,2,4,8,128,128};

	private:
		void setBank(int regio, byte* value);
		int romSize;
		int mapperType;
		byte mapperMask;
		byte *internalMemoryBank[8]; // 4 blocks of 8kB starting at #4000
		bool enabledSRAM;
		byte *memorySRAM; // SRAM area of Hydlide2/Xanadu/Royal Blood/Gamemaster2
		byte SRAMEnableBit;
		word maskSRAM;
		byte regioSRAM; //bit 0 if SRAM in 0x4000-0x5FFF,1 for 0x6000-0x7FFF,...

		int retrieveMapperType();
		int guessMapperType();
#ifndef DONT_WANT_SCC
		SCC* cartridgeSCC;
		bool enabledSCC;
#endif
		DACSound* dac;
};
#endif

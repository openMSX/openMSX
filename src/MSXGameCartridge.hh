// $Id$

#ifndef __MSXGAMECARTRIDGE_HH__
#define __MSXGAMECARTRIDGE_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXRomDevice.hh"
#include "MSXMemDevice.hh"

// forward declarations
#ifndef DONT_WANT_SCC
class SCC;
#endif
class DACSound;
class EmuTime;


class MSXGameCartridge : public MSXMemDevice, public MSXRomDevice
{
	public:
		MSXGameCartridge(MSXConfig::Device *config, const EmuTime &time);
		~MSXGameCartridge();

		void reset(const EmuTime &time);
		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);
		byte* getReadCacheLine(word start);

	private:
		void retrieveMapperType();
		int guessMapperType();
		
		inline void setBank4kB (int region, byte* adr);
		inline void setBank8kB (int region, byte* adr);
		inline void setBank16kB(int region, byte* adr);
		inline void setROM4kB  (int region, int block);
		inline void setROM8kB  (int region, int block);
		inline void setROM16kB (int region, int block);
		
		int romSize;
		int mapperType;
		byte *internalMemoryBank[16];	// 16 blocks of 4kB
		byte *unmapped;
		
		byte *memorySRAM;
		byte regioSRAM;	//bit n=1 => SRAM in [n*0x2000, (n+1)*0x2000)

#ifndef DONT_WANT_SCC
		SCC* cartridgeSCC;
		bool enabledSCC;
#endif
		DACSound* dac;
};

#endif

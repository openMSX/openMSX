// $Id$

#ifndef __MSXGAMECARTRIDGE_HH__
#define __MSXGAMECARTRIDGE_HH__

#include "MSXRom.hh"
#include "MSXMemDevice.hh"
#include "EmuTime.hh"
#include "SCC.hh"
#include "DACSound.hh"

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

	private:
		int romSize;
		int mapperType;
		byte mapperMask;
		byte *internalMemoryBank[8]; // 4 blocks of 8kB starting at #4000
		bool enabledSCC;

		int retriefMapperType();
		int guessMapperType();
		SCC* cartridgeSCC;
		DACSound* dac;
};
#endif

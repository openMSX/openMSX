// $Id$

#ifndef __MSXKonamiSynthesizer_HH__
#define __MSXKonamiSynthesizer_HH__

#include "MSXDevice.hh"
#include "DACSound.hh"
#include "EmuTime.hh"

class MSXKonamiSynthesizer : public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXKonamiSynthesizer(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXKonamiSynthesizer();
		
		void init();
		
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);

	private:
		byte* memoryBank;
		DACSound *DAC;
};
#endif

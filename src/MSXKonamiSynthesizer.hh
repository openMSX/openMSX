// $Id$

#ifndef __MSXKonamiSynthesizer_HH__
#define __MSXKonamiSynthesizer_HH__

#include "MSXDevice.hh"
#include "DACSound.hh"
#include "emutime.hh"

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
		
		// don't forget you inherited from MSXDevice
		void init();
		
		byte readMem(word address, Emutime &time);
		void writeMem(word address, byte value, Emutime &time);

	private:
		static const int ROM_SIZE = 2*16384;
		byte* memoryBank;
		DACSound *DAC;
};
#endif

// $Id$

#ifndef __MSXROM16KB_HH__
#define __MSXROM16KB_HH__

#include "MSXDevice.hh"
#include "EmuTime.hh"

class MSXRom16KB : public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXRom16KB(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXRom16KB();
		
		// don't forget you inherited from MSXDevice
		void init();
		
		byte readMem(word address, EmuTime &time);

	private:
		byte* memoryBank;
};
#endif

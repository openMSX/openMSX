// $Id$

#ifndef __MSXROM16KB_HH__
#define __MSXROM16KB_HH__

#include "MSXMemDevice.hh"
#include "MSXRom.hh"
#include "EmuTime.hh"
#include "msxconfig.hh"

class MSXRom16KB : public MSXMemDevice, public MSXRom
{
	public:
		/**
		 * Constructor
		 */
		MSXRom16KB(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXRom16KB();
		
		byte readMem(word address, const EmuTime &time);
};
#endif

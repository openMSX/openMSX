// $Id$

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#include "MSXRom.hh"
#include "MSXYM2413.hh"
#include "MSXMemDevice.hh"
#include "EmuTime.hh"


class MSXMusic : public MSXYM2413, public MSXMemDevice, public MSXRom
{
	public:
		/**
		 * Constructor
		 */
		MSXMusic(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXMusic(); 
		
		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);
};
#endif

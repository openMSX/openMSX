// $Id$

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXRomDevice.hh"
#include "MSXYM2413.hh"
#include "MSXMemDevice.hh"


class MSXMusic : public MSXYM2413, public MSXMemDevice, public MSXRomDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXMusic(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXMusic();
		
		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
};

#endif

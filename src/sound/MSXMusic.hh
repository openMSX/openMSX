// $Id$

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#ifndef VERSION
#include "config.h"
#endif

#ifndef DONT_WANT_MSXMUSIC

#include "MSXRom.hh"
#include "MSXYM2413.hh"
#include "MSXMemDevice.hh"

// forward declarations
class EmuTime;


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
		virtual ~MSXMusic();
		
		virtual void reset(const EmuTime &time);

		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
};

#endif // ndef DONT_WANT_MSXMUSIC

#endif

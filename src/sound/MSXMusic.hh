// $Id$

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXYM2413.hh"
#include "MSXMemDevice.hh"


class MSXMusic : public MSXYM2413
{
	public:
		MSXMusic(Device *config, const EmuTime &time);
		virtual ~MSXMusic();
		
		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
};

#endif

// $Id$

#ifndef __ROMPLAIN_HH__
#define __ROMPLAIN_HH__

#include "Rom16kBBlocks.hh"


class RomPlain : public Rom16kBBlocks
{
	public:
		RomPlain(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomPlain();

	private:
		void guessHelper(word offset, int* pages);
		word guessLocation();
};

#endif

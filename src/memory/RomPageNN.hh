// $Id$

#ifndef __ROMPAGENN_HH__
#define __ROMPAGENN_HH__

#include "Rom16kBBlocks.hh"


class RomPageNN : public Rom16kBBlocks
{
	public:
		RomPageNN(Device* config, const EmuTime &time, byte pages);
		virtual ~RomPageNN();
};

#endif

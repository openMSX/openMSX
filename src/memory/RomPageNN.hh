// $Id$

#ifndef __ROMPAGENN_HH__
#define __ROMPAGENN_HH__

#include "Rom16kBBlocks.hh"


namespace openmsx {

class RomPageNN : public Rom16kBBlocks
{
	public:
		RomPageNN(Device* config, const EmuTime &time, Rom *rom, byte pages);
		virtual ~RomPageNN();
};

} // namespace openmsx

#endif

// $Id$

#ifndef __ROMPLAIN_HH__
#define __ROMPLAIN_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomPlain : public Rom8kBBlocks
{
public:
	RomPlain(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomPlain();

private:
	void guessHelper(word offset, int* pages);
	word guessLocation();
};

} // namespace openmsx

#endif

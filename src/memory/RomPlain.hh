// $Id$

#ifndef __ROMPLAIN_HH__
#define __ROMPLAIN_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class XMLElement;

class RomPlain : public Rom8kBBlocks
{
public:
	enum MirrorType { MIRRORED, NOT_MIRRORED };

	RomPlain(const XMLElement& config, const EmuTime& time,
	         std::auto_ptr<Rom> rom, MirrorType mirrored, int start = -1);
	virtual ~RomPlain();

private:
	void guessHelper(unsigned offset, int* pages);
	unsigned guessLocation(unsigned windowBase, unsigned windowSize);
};

} // namespace openmsx

#endif

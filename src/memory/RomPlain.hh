// $Id$

#ifndef ROMPLAIN_HH
#define ROMPLAIN_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class XMLElement;

class RomPlain : public Rom8kBBlocks
{
public:
	enum MirrorType { MIRRORED, NOT_MIRRORED };

	RomPlain(MSXMotherBoard& motherBoard, const XMLElement& config,
	         const EmuTime& time, std::auto_ptr<Rom> rom,
	         MirrorType mirrored, int start = -1);

private:
	void guessHelper(unsigned offset, int* pages);
	unsigned guessLocation(unsigned windowBase, unsigned windowSize);
};

} // namespace openmsx

#endif

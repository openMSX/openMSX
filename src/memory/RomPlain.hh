// $Id$

#ifndef ROMPLAIN_HH
#define ROMPLAIN_HH

#include "RomBlocks.hh"

namespace openmsx {

class XMLElement;

class RomPlain : public Rom8kBBlocks
{
public:
	enum MirrorType { MIRRORED, NOT_MIRRORED };

	RomPlain(MSXMotherBoard& motherBoard, const XMLElement& config,
	         std::auto_ptr<Rom> rom, MirrorType mirrored, int start = -1);

private:
	void guessHelper(unsigned offset, int* pages);
	unsigned guessLocation(unsigned windowBase, unsigned windowSize);
};

REGISTER_MSXDEVICE(RomPlain, "RomPlain");

} // namespace openmsx

#endif

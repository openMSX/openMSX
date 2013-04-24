#ifndef ROMPLAIN_HH
#define ROMPLAIN_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomPlain : public Rom8kBBlocks
{
public:
	enum MirrorType { MIRRORED, NOT_MIRRORED };

	RomPlain(const DeviceConfig& config, std::unique_ptr<Rom> rom,
	         MirrorType mirrored, int start = -1);

private:
	void guessHelper(unsigned offset, int* pages);
	unsigned guessLocation(unsigned windowBase, unsigned windowSize);
};

} // namespace openmsx

#endif

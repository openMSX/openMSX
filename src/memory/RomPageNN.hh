#ifndef ROMPAGENN_HH
#define ROMPAGENN_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomPageNN final : public Rom8kBBlocks
{
public:
	RomPageNN(const DeviceConfig& config, Rom&& rom, byte pages);
};

} // namespace openmsx

#endif

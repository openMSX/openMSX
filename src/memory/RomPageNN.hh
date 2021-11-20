#ifndef ROMPAGENN_HH
#define ROMPAGENN_HH

#include "RomBlocks.hh"
#include "RomTypes.hh"

namespace openmsx {

class RomPageNN final : public Rom8kBBlocks
{
public:
	RomPageNN(const DeviceConfig& config, Rom&& rom, RomType type);
};

} // namespace openmsx

#endif

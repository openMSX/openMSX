#ifndef ROMNAMCO_HH
#define ROMNAMCO_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomNamco final : public Rom8kBBlocks
{
public:
	RomNamco(const DeviceConfig& config, Rom&& rom);
};

} // namespace openmsx

#endif

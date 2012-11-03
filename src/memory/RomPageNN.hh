// $Id$

#ifndef ROMPAGENN_HH
#define ROMPAGENN_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomPageNN : public Rom8kBBlocks
{
public:
	RomPageNN(const DeviceConfig& config, std::unique_ptr<Rom> rom, byte pages);
};

} // namespace openmsx

#endif

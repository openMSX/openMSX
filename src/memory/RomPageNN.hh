// $Id$

#ifndef ROMPAGENN_HH
#define ROMPAGENN_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomPageNN : public Rom8kBBlocks
{
public:
	RomPageNN(MSXMotherBoard& motherBoard, const XMLElement& config,
	          const EmuTime& time, std::auto_ptr<Rom> rom, byte pages);
};

} // namespace openmsx

#endif

// $Id$

#ifndef ROMPAGENN_HH
#define ROMPAGENN_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomPageNN : public Rom8kBBlocks
{
public:
	RomPageNN(MSXMotherBoard& motherBoard, const XMLElement& config,
	          std::auto_ptr<Rom> rom, byte pages);
};

REGISTER_MSXDEVICE(RomPageNN, "RomPageNN");

} // namespace openmsx

#endif

// $Id$

#ifndef __ROMPAGENN_HH__
#define __ROMPAGENN_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomPageNN : public Rom8kBBlocks
{
public:
	RomPageNN(const XMLElement& config, const EmuTime& time,
	          std::auto_ptr<Rom> rom, byte pages);
	virtual ~RomPageNN();
};

} // namespace openmsx

#endif // __ROMPAGENN_HH__

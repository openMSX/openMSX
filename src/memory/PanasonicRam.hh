// $Id$

#ifndef PANASONICRAM_HH
#define PANASONICRAM_HH

#include "MSXMemoryMapper.hh"

namespace openmsx {

class PanasonicRam : public MSXMemoryMapper
{
public:
	PanasonicRam(const XMLElement& config, const EmuTime& time);
	virtual ~PanasonicRam();
};

} // namespace openmsx

#endif

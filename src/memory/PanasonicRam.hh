// $Id$

#ifndef __PANASONICRAM_HH__
#define __PANASONICRAM_HH__

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

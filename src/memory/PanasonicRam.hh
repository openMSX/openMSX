// $Id$

#ifndef PANASONICRAM_HH
#define PANASONICRAM_HH

#include "MSXMemoryMapper.hh"

namespace openmsx {

class PanasonicRam : public MSXMemoryMapper
{
public:
	PanasonicRam(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time);
};

} // namespace openmsx

#endif

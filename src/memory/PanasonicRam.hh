// $Id$

#ifndef PANASONICRAM_HH
#define PANASONICRAM_HH

#include "MSXMemoryMapper.hh"

namespace openmsx {

class PanasonicMemory;

class PanasonicRam : public MSXMemoryMapper
{
public:
	PanasonicRam(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time);

	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word start) const;

private:
	PanasonicMemory& panasonicMemory;
};

} // namespace openmsx

#endif

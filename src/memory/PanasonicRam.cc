// $Id$

#include "PanasonicRam.hh"
#include "MSXMotherBoard.hh"
#include "PanasonicMemory.hh"
#include "CheckedRam.hh"

namespace openmsx {

PanasonicRam::PanasonicRam(MSXMotherBoard& motherBoard,
                           const XMLElement& config, const EmuTime& time)
	: MSXMemoryMapper(motherBoard, config, time)
	, panasonicMemory(motherBoard.getPanasonicMemory())
{
	panasonicMemory.registerRam(checkedRam->getUncheckedRam());
}

void PanasonicRam::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	unsigned addr = calcAddress(address);
	if (panasonicMemory.isWritable(addr)) {
		checkedRam->write(addr, value);
	}
}

byte* PanasonicRam::getWriteCacheLine(word start) const
{
	unsigned addr = calcAddress(start);
	if (panasonicMemory.isWritable(addr)) {
		return checkedRam->getWriteCacheLine(addr);
	} else {
		return NULL;
	}
}

} // namespace openmsx

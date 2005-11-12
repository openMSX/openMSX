// $Id$

#include "PanasonicRam.hh"
#include "MSXMotherBoard.hh"
#include "PanasonicMemory.hh"

namespace openmsx {

PanasonicRam::PanasonicRam(MSXMotherBoard& motherBoard,
                           const XMLElement& config, const EmuTime& time)
	: MSXMemoryMapper(motherBoard, config, time)
{
	motherBoard.getPanasonicMemory().registerRam(*ram);
}

} // namespace openmsx

// $Id$

#include "PanasonicRam.hh"
#include "PanasonicMemory.hh"

namespace openmsx {

PanasonicRam::PanasonicRam(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXMemoryMapper(config, time)
{
	PanasonicMemory::instance().registerRam(*ram);
}

PanasonicRam::~PanasonicRam()
{
}

} // namespace openmsx

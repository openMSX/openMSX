// $Id$

#include "PanasonicRam.hh"
#include "PanasonicMemory.hh"


PanasonicRam::PanasonicRam(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemoryMapper(config, time)
{
	PanasonicMemory::instance()->registerRam(buffer, 0x4000 * nbBlocks);
}

PanasonicRam::~PanasonicRam()
{
}

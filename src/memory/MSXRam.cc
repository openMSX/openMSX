// $Id$

#include "MSXRam.hh"
#include "CPU.hh"
#include "xmlx.hh"
#include "Ram.hh"

namespace openmsx {

MSXRam::MSXRam(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	// slow drain on reset
	slowDrainOnReset = config.getChildDataAsBool("slow_drain_on_reset", false);

	// size / base
	int s = config.getChildDataAsInt("size", 64);
	int b = config.getChildDataAsInt("base", 0);
	if ((s <= 0) || (s > 64)) {
		throw FatalError("Wrong RAM size");
	}
	if ((b < 0) || ((b + s) > 64)) {
		throw FatalError("Wrong RAM base");
	}
	base = b * 1024;
	int size = s * 1024;
	end = base + size;
	
	assert(CPU::CACHE_LINE_SIZE <= 1024);	// size must be cache aligned
	
	ram.reset(new Ram(getName(), "ram", size));
}

MSXRam::~MSXRam()
{
}

void MSXRam::reset(const EmuTime& time)
{
	if (!slowDrainOnReset) {
		ram->clear();
	}
}

bool MSXRam::isInside(word address) const
{
	return ((base <= address) && (address < end));
}

byte MSXRam::readMem(word address, const EmuTime& time)
{
	if (isInside(address)) {
		return (*ram)[address - base];
	} else {
		return 0xFF;
	}
}

void MSXRam::writeMem(word address, byte value, const EmuTime& time)
{
	if (isInside(address)) {
		(*ram)[address - base] = value;
	} else {
		// ignore
	}
}

const byte* MSXRam::getReadCacheLine(word start) const
{
	if (isInside(start)) {
		return &(*ram)[start - base];
	} else {
		return unmappedRead;
	}
}

byte* MSXRam::getWriteCacheLine(word start) const
{
	if (isInside(start)) {
		return &(*ram)[start - base];
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx

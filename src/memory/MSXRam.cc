// $Id$

#include "MSXRam.hh"
#include "CPU.hh"
#include "Ram.hh"
#include "XMLElement.hh"
#include "MSXException.hh"

namespace openmsx {

MSXRam::MSXRam(MSXMotherBoard& motherBoard, const XMLElement& config,
               const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
{
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

void MSXRam::powerUp(const EmuTime& /*time*/)
{
	ram->clear();
}

bool MSXRam::isInside(word address) const
{
	return ((base <= address) && (address < end));
}

byte MSXRam::readMem(word address, const EmuTime& /*time*/)
{
	if (isInside(address)) {
		return (*ram)[address - base];
	} else {
		return 0xFF;
	}
}

void MSXRam::writeMem(word address, byte value, const EmuTime& /*time*/)
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

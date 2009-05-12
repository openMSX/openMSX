// $Id$

#include "MSXRam.hh"
#include "CheckedRam.hh"
#include "serialize.hh"
#include <cassert>

#include "Ram.hh" // because we serialize Ram instead of CheckedRam

namespace openmsx {

MSXRam::MSXRam(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
	// Actual initialization is done in init() because <mem> tags
	// are not yet processed.
}

void MSXRam::init(const HardwareConfig& hwConf)
{
	MSXDevice::init(hwConf); // parse mem regions

	getVisibleMemRegion(base, size);
	assert( base         <  0x10000);
	assert((base + size) <= 0x10000);

	checkedRam.reset(new CheckedRam(
		getMotherBoard(), getName(), "ram", size));
}

void MSXRam::powerUp(EmuTime::param /*time*/)
{
	checkedRam->clear();
}

unsigned MSXRam::translate(unsigned address) const
{
	assert((address - base) < size);
	return address - base;
}

byte MSXRam::peekMem(word address, EmuTime::param /*time*/) const
{
	return checkedRam->peek(translate(address));
}

byte MSXRam::readMem(word address, EmuTime::param /*time*/)
{
	return checkedRam->read(translate(address));
}

void MSXRam::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	checkedRam->write(translate(address), value);
}

const byte* MSXRam::getReadCacheLine(word start) const
{
	return checkedRam->getReadCacheLine(translate(start));
}

byte* MSXRam::getWriteCacheLine(word start) const
{
	return checkedRam->getWriteCacheLine(translate(start));
}

template<typename Archive>
void MSXRam::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// TODO ar.serialize("checkedRam", checkedRam);
	ar.serialize("ram", checkedRam->getUncheckedRam());
}
INSTANTIATE_SERIALIZE_METHODS(MSXRam);
REGISTER_MSXDEVICE(MSXRam, "Ram");

} // namespace openmsx

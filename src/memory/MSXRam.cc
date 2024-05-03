#include "MSXRam.hh"
#include "XMLElement.hh"
#include "narrow.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

MSXRam::MSXRam(const DeviceConfig& config)
	: MSXDevice(config)
{
	// Actual initialization is done in init() because <mem> tags
	// are not yet processed.
}

void MSXRam::init()
{
	MSXDevice::init(); // parse mem regions

	// by default get base/size from the (union of) the <mem> tag(s)
	getVisibleMemRegion(base, size);
	// but allow to override these two parameters
	base = getDeviceConfig().getChildDataAsInt("base", narrow_cast<int>(base));
	size = getDeviceConfig().getChildDataAsInt("size", narrow_cast<int>(size));
	assert( base         <  0x10000);
	assert((base + size) <= 0x10000);

	checkedRam.emplace(getDeviceConfig2(), getName(), "ram", size);
}

void MSXRam::powerUp(EmuTime::param /*time*/)
{
	checkedRam->clear();
}

unsigned MSXRam::translate(unsigned address) const
{
	address -= base;
	if (address >= size) address &= (size - 1);
	return address;
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

byte* MSXRam::getWriteCacheLine(word start)
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

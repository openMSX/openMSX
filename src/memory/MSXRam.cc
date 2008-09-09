// $Id$

#include "MSXRam.hh"
#include "CacheLine.hh"
#include "CheckedRam.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "serialize.hh"

#include "Ram.hh" // because we serialize Ram instead of CheckedRam

namespace openmsx {

MSXRam::MSXRam(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, base(config.getChildDataAsInt("base", 0))
	, size(config.getChildDataAsInt("size", 0x10000))
	, checkedRam(new CheckedRam(motherBoard, getName(), "ram", size))
{
	if ((size > 0x10000) || (base >= 0x10000)) {
		throw MSXException("Invalid base/size for " + getName() +
		                   ", must be in range [0x0000,0x10000).");
	}
	if ((base & CacheLine::LOW) || (size & CacheLine::LOW)) {
		throw MSXException("Invalid base/size alignment for " +
		                   getName());
	}
}

void MSXRam::powerUp(const EmuTime& /*time*/)
{
	checkedRam->clear();
}

word MSXRam::translate(word address) const
{
	word tmp = address - base;
	return (tmp < size) ? tmp : tmp & (size - 1);
}

byte MSXRam::peekMem(word address, const EmuTime& /*time*/) const
{
	return checkedRam->peek(translate(address));
}

byte MSXRam::readMem(word address, const EmuTime& /*time*/)
{
	return checkedRam->read(translate(address));
}

void MSXRam::writeMem(word address, byte value, const EmuTime& /*time*/)
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

} // namespace openmsx

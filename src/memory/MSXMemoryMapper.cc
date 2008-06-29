// $Id$

#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "XMLElement.hh"
#include "DeviceFactory.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "CheckedRam.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cassert>

#include "Ram.hh" //

namespace openmsx {

unsigned MSXMemoryMapper::calcAddress(word address) const
{
	unsigned page = mapperIO->getSelectedPage(address >> 14);
	page = (page < nbBlocks) ? page : page & (nbBlocks - 1);
	return (page << 14) | (address & 0x3FFF);
}

MSXMemoryMapper::MSXMemoryMapper(MSXMotherBoard& motherBoard,
                                 const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
	int kSize = config.getChildDataAsInt("size");
	if ((kSize % 16) != 0) {
		throw MSXException("Mapper size is not a multiple of 16K: " +
		                   StringOp::toString(kSize));
	}
	nbBlocks = kSize / 16;
	checkedRam.reset(new CheckedRam(motherBoard, getName(), "memory mapper",
	                                nbBlocks * 0x4000));

	mapperIO = motherBoard.createMapperIO();
	mapperIO->registerMapper(nbBlocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	mapperIO->unregisterMapper(nbBlocks);
	getMotherBoard().destroyMapperIO();
}

void MSXMemoryMapper::powerUp(const EmuTime& /*time*/)
{
	checkedRam->clear();
}

void MSXMemoryMapper::reset(const EmuTime& time)
{
	mapperIO->reset(time);
}

byte MSXMemoryMapper::peekMem(word address, const EmuTime& /*time*/) const
{
	return checkedRam->peek(calcAddress(address));
}

byte MSXMemoryMapper::readMem(word address, const EmuTime& /*time*/)
{
	return checkedRam->read(calcAddress(address));
}

void MSXMemoryMapper::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	checkedRam->write(calcAddress(address), value);
}

const byte* MSXMemoryMapper::getReadCacheLine(word start) const
{
	return checkedRam->getReadCacheLine(calcAddress(start));
}

byte* MSXMemoryMapper::getWriteCacheLine(word start) const
{
	return checkedRam->getWriteCacheLine(calcAddress(start));
}


template<typename Archive>
void MSXMemoryMapper::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// TODO ar.serialize("checkedRam", checkedRam);
	ar.serialize("ram", checkedRam->getUncheckedRam());
}
INSTANTIATE_SERIALIZE_METHODS(MSXMemoryMapper);

} // namespace openmsx

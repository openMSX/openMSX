#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "memory.hh"
#include "Ram.hh" // because we serialize Ram instead of CheckedRam

namespace openmsx {

unsigned MSXMemoryMapper::getRamSize() const
{
	int kSize = getDeviceConfig().getChildDataAsInt("size");
	if ((kSize % 16) != 0) {
		throw MSXException(StringOp::Builder() <<
			"Mapper size is not a multiple of 16K: " << kSize);
	}
	return kSize * 1024; // in bytes
}

MSXMemoryMapper::MSXMemoryMapper(const DeviceConfig& config)
	: MSXDevice(config)
	, checkedRam(config, getName(), "memory mapper", getRamSize())
	, mapperIO(*getMotherBoard().createMapperIO())
{
	unsigned nbBlocks = checkedRam.getSize() / 0x4000;
	mapperIO.registerMapper(nbBlocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	unsigned nbBlocks = checkedRam.getSize() / 0x4000;
	mapperIO.unregisterMapper(nbBlocks);
	getMotherBoard().destroyMapperIO();
}

void MSXMemoryMapper::powerUp(EmuTime::param time)
{
	checkedRam.clear();
	reset(time);
}

void MSXMemoryMapper::reset(EmuTime::param time)
{
	mapperIO.reset(time);
}

unsigned MSXMemoryMapper::calcAddress(word address) const
{
	unsigned page = mapperIO.getSelectedPage(address >> 14);
	unsigned nbBlocks = checkedRam.getSize() / 0x4000;
	page = (page < nbBlocks) ? page : page & (nbBlocks - 1);
	return (page << 14) | (address & 0x3FFF);
}

byte MSXMemoryMapper::peekMem(word address, EmuTime::param /*time*/) const
{
	return checkedRam.peek(calcAddress(address));
}

byte MSXMemoryMapper::readMem(word address, EmuTime::param /*time*/)
{
	return checkedRam.read(calcAddress(address));
}

void MSXMemoryMapper::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	checkedRam.write(calcAddress(address), value);
}

const byte* MSXMemoryMapper::getReadCacheLine(word start) const
{
	return checkedRam.getReadCacheLine(calcAddress(start));
}

byte* MSXMemoryMapper::getWriteCacheLine(word start) const
{
	return checkedRam.getWriteCacheLine(calcAddress(start));
}

template<typename Archive>
void MSXMemoryMapper::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// TODO ar.serialize("checkedRam", checkedRam);
	ar.serialize("ram", checkedRam.getUncheckedRam());
}
INSTANTIATE_SERIALIZE_METHODS(MSXMemoryMapper);
REGISTER_MSXDEVICE(MSXMemoryMapper, "MemoryMapper");

} // namespace openmsx

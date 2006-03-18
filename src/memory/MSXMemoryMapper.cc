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
#include <cassert>

namespace openmsx {

unsigned MSXMemoryMapper::counter = 0;
MSXMapperIO* MSXMemoryMapper::mapperIO = NULL;

unsigned MSXMemoryMapper::calcAddress(word address) const
{
	unsigned page = mapperIO->getSelectedPage(address >> 14);
	page = (page < nbBlocks) ? page : page & (nbBlocks - 1);
	return (page << 14) | (address & 0x3FFF);
}

MSXMemoryMapper::MSXMemoryMapper(MSXMotherBoard& motherBoard,
                                 const XMLElement& config, const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
{
	int kSize = deviceConfig.getChildDataAsInt("size");
	if ((kSize % 16) != 0) {
		throw MSXException("Mapper size is not a multiple of 16K: " +
		                   StringOp::toString(kSize));
	}
	nbBlocks = kSize / 16;
	checkedRam.reset(new CheckedRam(motherBoard, getName(), "memory mapper",
	                                nbBlocks * 0x4000));

	createMapperIO(motherBoard);
	mapperIO->registerMapper(nbBlocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	mapperIO->unregisterMapper(nbBlocks);
	destroyMapperIO();
}

void MSXMemoryMapper::powerUp(const EmuTime& /*time*/)
{
	checkedRam->clear();
}

void MSXMemoryMapper::createMapperIO(MSXMotherBoard& motherBoard)
{
	if (!counter) {
		assert(!mapperIO);
		mapperIO = DeviceFactory::createMapperIO(motherBoard).release();

		MSXCPUInterface& cpuInterface = getMotherBoard().getCPUInterface();
		cpuInterface.register_IO_Out(0xFC, mapperIO);
		cpuInterface.register_IO_Out(0xFD, mapperIO);
		cpuInterface.register_IO_Out(0xFE, mapperIO);
		cpuInterface.register_IO_Out(0xFF, mapperIO);
		cpuInterface.register_IO_In(0xFC, mapperIO);
		cpuInterface.register_IO_In(0xFD, mapperIO);
		cpuInterface.register_IO_In(0xFE, mapperIO);
		cpuInterface.register_IO_In(0xFF, mapperIO);
	}
	++counter;
}

void MSXMemoryMapper::destroyMapperIO()
{
	--counter;
	if (!counter) {
		MSXCPUInterface& cpuInterface = getMotherBoard().getCPUInterface();
		cpuInterface.unregister_IO_Out(0xFC, mapperIO);
		cpuInterface.unregister_IO_Out(0xFD, mapperIO);
		cpuInterface.unregister_IO_Out(0xFE, mapperIO);
		cpuInterface.unregister_IO_Out(0xFF, mapperIO);
		cpuInterface.unregister_IO_In(0xFC, mapperIO);
		cpuInterface.unregister_IO_In(0xFD, mapperIO);
		cpuInterface.unregister_IO_In(0xFE, mapperIO);
		cpuInterface.unregister_IO_In(0xFF, mapperIO);

		assert(mapperIO);
		delete mapperIO;
		mapperIO = NULL;
	}
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

} // namespace openmsx

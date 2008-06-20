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

	createMapperIO();
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

void MSXMemoryMapper::createMapperIO()
{
	MSXMotherBoard::SharedStuff& info =
		getMotherBoard().getSharedStuff("MSXMapperIO");
	if (info.counter == 0) {
		assert(info.stuff == NULL);
		MSXMapperIO* device = DeviceFactory::createMapperIO(
		                                 getMotherBoard()).release();
		info.stuff = device;

		MSXCPUInterface& cpuInterface = getMotherBoard().getCPUInterface();
		cpuInterface.register_IO_Out(0xFC, device);
		cpuInterface.register_IO_Out(0xFD, device);
		cpuInterface.register_IO_Out(0xFE, device);
		cpuInterface.register_IO_Out(0xFF, device);
		cpuInterface.register_IO_In (0xFC, device);
		cpuInterface.register_IO_In (0xFD, device);
		cpuInterface.register_IO_In (0xFE, device);
		cpuInterface.register_IO_In (0xFF, device);
	}
	++info.counter;
	mapperIO = reinterpret_cast<MSXMapperIO*>(info.stuff);
}

void MSXMemoryMapper::destroyMapperIO()
{
	MSXMotherBoard::SharedStuff& info =
		getMotherBoard().getSharedStuff("MSXMapperIO");
	assert(mapperIO);
	assert(mapperIO == info.stuff);
	assert(info.counter);
	--info.counter;
	if (info.counter == 0) {
		MSXCPUInterface& cpuInterface = getMotherBoard().getCPUInterface();
		cpuInterface.unregister_IO_Out(0xFC, mapperIO);
		cpuInterface.unregister_IO_Out(0xFD, mapperIO);
		cpuInterface.unregister_IO_Out(0xFE, mapperIO);
		cpuInterface.unregister_IO_Out(0xFF, mapperIO);
		cpuInterface.unregister_IO_In (0xFC, mapperIO);
		cpuInterface.unregister_IO_In (0xFD, mapperIO);
		cpuInterface.unregister_IO_In (0xFE, mapperIO);
		cpuInterface.unregister_IO_In (0xFF, mapperIO);

		delete mapperIO;
		info.stuff = NULL;
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

// $Id$

#include <cassert>
#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "Config.hh"
#include "MSXCPUInterface.hh"
#include "MSXConfig.hh"
#include "Debugger.hh"
#include "FileContext.hh"

namespace openmsx {

unsigned MSXMemoryMapper::counter = 0;
Config* MSXMemoryMapper::device = NULL;
MSXMapperIO* MSXMemoryMapper::mapperIO = NULL;

// Inlined methods first, to make sure they are actually inlined
inline unsigned MSXMemoryMapper::calcAddress(word address) const
{
	unsigned page = mapperIO->getSelectedPage(address >> 14);
	page = (page < nbBlocks) ? page : page & (nbBlocks - 1);
	return (page << 14) | (address & 0x3FFF);
}

MSXMemoryMapper::MSXMemoryMapper(Config* config, const EmuTime& time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset", false);
	int kSize = deviceConfig->getParameterAsInt("size");
	if ((kSize % 16) != 0) {
		ostringstream out;
		out << "Mapper size is not a multiple of 16K: " << kSize;
		throw FatalError(out.str());
	}
	nbBlocks = kSize / 16;
	ram = new Ram(getName(), "memory mapper", nbBlocks * 0x4000);

	createMapperIO(time);
	mapperIO->registerMapper(nbBlocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	mapperIO->unregisterMapper(nbBlocks); 
	destroyMapperIO();
	 
	delete ram;
}

void MSXMemoryMapper::createMapperIO(const EmuTime& time)
{
	if (!counter) {
		assert(!mapperIO && !device);

		XMLElement deviceElem("MapperIO");
		deviceElem.addChild(
			auto_ptr<XMLElement>(new XMLElement("type", "MapperIO")));
		SystemFileContext dummyContext;
		device = new Config(deviceElem, dummyContext);
		mapperIO = new MSXMapperIO(device, time);
	
		MSXCPUInterface& cpuInterface = MSXCPUInterface::instance();
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
		assert(mapperIO && device);
		delete mapperIO;
		delete device;
	}
}

void MSXMemoryMapper::reset(const EmuTime& time)
{
	if (!slowDrainOnReset) {
		ram->clear();
	}
	mapperIO->reset(time);
}

byte MSXMemoryMapper::readMem(word address, const EmuTime& time)
{
	return (*ram)[calcAddress(address)];
}

void MSXMemoryMapper::writeMem(word address, byte value, const EmuTime& time)
{
	(*ram)[calcAddress(address)] = value;
}

const byte* MSXMemoryMapper::getReadCacheLine(word start) const
{
	return &(*ram)[calcAddress(start)];
}

byte* MSXMemoryMapper::getWriteCacheLine(word start) const
{
	return &(*ram)[calcAddress(start)];
}

} // namespace openmsx

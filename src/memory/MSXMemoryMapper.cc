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
	buffer = new byte[nbBlocks * 0x4000];
	// Isn't completely true, but let's suppose that ram will
	// always contain all zero if started
	memset(buffer, 0, nbBlocks * 0x4000);

	createMapperIO(time);
	mapperIO->registerMapper(nbBlocks);

	Debugger::instance().registerDebuggable(deviceConfig->getId(), *this);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	Debugger::instance().unregisterDebuggable(deviceConfig->getId(), *this);

	mapperIO->unregisterMapper(nbBlocks); 
	destroyMapperIO();
	 
	delete[] buffer;
}

void MSXMemoryMapper::createMapperIO(const EmuTime& time)
{
	if (!counter) {
		assert(!mapperIO && !device);

		XMLElement deviceElem("MapperIO");
		XMLElement* typeElem = new XMLElement("type", "MapperIO");
		deviceElem.addChild(typeElem);
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
		PRT_DEBUG("Clearing ram of " << getName());
		memset(buffer, 0, nbBlocks * 0x4000);
	}
	mapperIO->reset(time);
}

byte MSXMemoryMapper::readMem(word address, const EmuTime& time)
{
	return buffer[calcAddress(address)];
}

void MSXMemoryMapper::writeMem(word address, byte value, const EmuTime& time)
{
	buffer[calcAddress(address)] = value;
}

const byte* MSXMemoryMapper::getReadCacheLine(word start) const
{
	return &buffer[calcAddress(start)];
}

byte* MSXMemoryMapper::getWriteCacheLine(word start) const
{
	return &buffer[calcAddress(start)];
}


// Debuggable

unsigned MSXMemoryMapper::getSize() const
{
	return nbBlocks * 0x4000;
}

const string& MSXMemoryMapper::getDescription() const
{
	static const string desc = "Memory Mapper.";
	return desc;
}

byte MSXMemoryMapper::read(unsigned address)
{
	return buffer[address];
}

void MSXMemoryMapper::write(unsigned address, byte value)
{
	buffer[address] = value;
}

} // namespace openmsx

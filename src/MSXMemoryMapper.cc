// $Id$

#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"


// Inlined methods first, to make sure they are actually inlined
inline int MSXMemoryMapper::calcAddress(word address)
{
	int page = mapperIO->getSelectedPage(address >> 14);
	page = (page < nbBlocks) ? page : page & (nbBlocks-1);
	return (page << 14) | (address & 0x3FFF);
}


MSXMemoryMapper::MSXMemoryMapper(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	PRT_DEBUG("Creating an MSXMemoryMapper object");

	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	int kSize = deviceConfig->getParameterAsInt("size");
	if (kSize % 16 != 0) {
		PRT_ERROR("Mapper size is not a multiple of 16K: " << kSize);
	}
	nbBlocks = kSize/16;
	if (!(buffer = new byte[nbBlocks * 16384]))
		PRT_ERROR("Couldn't allocate memory for " << getName());
	// Isn't completely true, but let's suppose that ram will
	// always contain all zero if started
	memset(buffer, 0, nbBlocks * 16384);

	mapperIO = MSXMapperIO::instance();
	mapperIO->registerMapper(nbBlocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	PRT_DEBUG("Destructing an MSXMemoryMapper object");

	// TODO first fix dependencies between MSXMemoryMapper and MSXMapperIO
	//mapperIO->unregisterMapper(nbBlocks); 
	delete[] buffer;
}

void MSXMemoryMapper::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(buffer, 0, nbBlocks * 16384);
	}
}

byte MSXMemoryMapper::readMem(word address, const EmuTime &time)
{
	return buffer[calcAddress(address)];
}

void MSXMemoryMapper::writeMem(word address, byte value, const EmuTime &time)
{
	buffer[calcAddress(address)] = value;
}

byte* MSXMemoryMapper::getReadCacheLine(word start)
{
	return &buffer[calcAddress(start)];
}

byte* MSXMemoryMapper::getWriteCacheLine(word start)
{
	return &buffer[calcAddress(start)];
}

// $Id$

#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "MSXConfig.hh"


// Inlined methods first, to make sure they are actually inlined
inline int MSXMemoryMapper::calcAddress(word address) const
{
	int page = mapperIO->getSelectedPage(address >> 14);
	page = (page < nbBlocks) ? page : page & (nbBlocks-1);
	return (page << 14) | (address & 0x3FFF);
}


MSXMemoryMapper::MSXMemoryMapper(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	int kSize = deviceConfig->getParameterAsInt("size");
	if ((kSize % 16) != 0)
		PRT_ERROR("Mapper size is not a multiple of 16K: " << kSize);
	nbBlocks = kSize / 16;
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
	// TODO first fix dependencies between MSXMemoryMapper and MSXMapperIO
	//      MSXMapperIO can be destroyed before MSXMemoryMapper is destroyed
	// mapperIO->unregisterMapper(nbBlocks); 
	delete[] buffer;
}

void MSXMemoryMapper::reset(const EmuTime &time)
{
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

const byte* MSXMemoryMapper::getReadCacheLine(word start) const
{
	return &buffer[calcAddress(start)];
}

byte* MSXMemoryMapper::getWriteCacheLine(word start) const
{
	return &buffer[calcAddress(start)];
}

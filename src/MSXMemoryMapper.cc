// $Id$

#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"


// Inlined methods first, to make sure they are actually inlined:
inline int MSXMemoryMapper::calcAddress(word address)
{
	// TODO: Keep pageAddr per mapper and apply mask on reg write.
	int base = (MSXMapperIO::pageAddr[address>>14] <= sizeMask) ? 
	           (MSXMapperIO::pageAddr[address>>14]) :
		   (MSXMapperIO::pageAddr[address>>14] & sizeMask);
	return base | (address & 0x3FFF);
}

MSXMemoryMapper::MSXMemoryMapper(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXMemoryMapper object");

	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	int kSize = deviceConfig->getParameterAsInt("size");
	if (kSize % 16 != 0) {
		PRT_ERROR("Mapper size is not a multiple of 16K: " << kSize);
	}
	int blocks = kSize/16;
	size = 16384 * blocks;
	sizeMask = size - 1; // Both convenient and correct!
	if (!(buffer = new byte[size]))
		PRT_ERROR("Couldn't allocate memory for " << getName());
	//Isn't completely true, but let's suppose that ram will
	//always contain all zero if started
	memset(buffer, 0, size);

	MSXMapperIO::instance()->registerMapper(blocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	PRT_DEBUG("Destructing an MSXMemoryMapper object");
	delete[] buffer;
	//TODO unregisterMapper
}

void MSXMemoryMapper::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(buffer, 0, size);
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

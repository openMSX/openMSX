// $Id$

#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"


MSXMemoryMapper::MSXMemoryMapper(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXMemoryMapper object");
	
	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	int kSize = deviceConfig->getParameterAsInt("size");
	blocks = kSize/16;
	int size = 16384*blocks;
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
	delete [] buffer; // C++ can handle null-pointers
}

void MSXMemoryMapper::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(buffer, 0, blocks*16384);
	}
}

byte MSXMemoryMapper::readMem(word address, EmuTime &time)
{
	return buffer[getAdr(address)];
}

void MSXMemoryMapper::writeMem(word address, byte value, EmuTime &time)
{
	buffer[getAdr(address)] = value;
}

int MSXMemoryMapper::getAdr(int address)
{
	int pageNum = MSXMapperIO::instance()->getPageNum(address>>14);
	pageNum %= blocks;
	int adr = pageNum*16384 + (address&0x3fff);
	return adr;
}


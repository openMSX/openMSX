// $Id$

#include "MSXSimple64KB.hh"


MSXSimple64KB::MSXSimple64KB(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXSimple64KB object");
	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	memoryBank = new byte[65536];
	if (memoryBank == NULL) 
		PRT_ERROR("Couldn't create 64KB memory bank !!!!!!");
	//Isn't completely true, but let's suppose that ram will 
	//always contain all zero if started
	memset(memoryBank, 0, 65536); // new doesn't fill with zero
}

MSXSimple64KB::~MSXSimple64KB()
{
	PRT_DEBUG("Destructing an MSXSimple64KB object");
	delete [] memoryBank; // C++ can handle null-pointers
}

void MSXSimple64KB::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(memoryBank, 0, 65536);
	}
}

byte MSXSimple64KB::readMem(word address, const EmuTime &time)
{
	return memoryBank [address];
}

void MSXSimple64KB::writeMem(word address, byte value, const EmuTime &time)
{
	memoryBank[address] = value;
}

byte* MSXSimple64KB::getReadCacheLine(word start)
{
	return &memoryBank[start];
}

byte* MSXSimple64KB::getWriteCacheLine(word start)
{
	return &memoryBank[start];
}

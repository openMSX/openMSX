// $Id$

#include "MSXSimple64KB.hh"


MSXSimple64KB::MSXSimple64KB(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	memoryBank = new byte[65536];
	//Isn't completely true, but let's suppose that ram will 
	//always contain all zero if started
	memset(memoryBank, 0, 65536);
}

MSXSimple64KB::~MSXSimple64KB()
{
	delete[] memoryBank;
}

void MSXSimple64KB::reset(const EmuTime &time)
{
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(memoryBank, 0, 65536);
	}
}

byte MSXSimple64KB::readMem(word address, const EmuTime &time)
{
	return memoryBank[address];
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

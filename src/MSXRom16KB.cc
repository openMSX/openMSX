// $Id$

#include "MSXRom16KB.hh" 
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"

MSXRom16KB::MSXRom16KB(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time), MSXRom(config, time)
{
	PRT_DEBUG("Creating an MSXRom16KB object");
	loadFile(&memoryBank, 0x4000);
}

MSXRom16KB::~MSXRom16KB()
{
	PRT_DEBUG("Destructing an MSXRom16KB object");
}

byte MSXRom16KB::readMem(word address, const EmuTime &time)
{
	return memoryBank [address & 0x3fff];
}

byte* MSXRom16KB::getReadCacheLine(word start)
{
	return &memoryBank[start & 0x3fff];
}

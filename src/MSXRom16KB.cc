// $Id$

#include "MSXRom16KB.hh" 
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"

MSXRom16KB::MSXRom16KB(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXRom16KB object");
	loadFile(&memoryBank, 0x4000);
}

MSXRom16KB::~MSXRom16KB()
{
	PRT_DEBUG("Destructing an MSXRom16KB object");
}

byte MSXRom16KB::readMem(word address, EmuTime &time)
{
	return memoryBank [address & 0x3fff];
}

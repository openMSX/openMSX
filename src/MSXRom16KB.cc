// $Id$

#include "MSXRom16KB.hh" 
#include "MSXMotherBoard.hh"
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"

MSXRom16KB::MSXRom16KB(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXRom16KB object");
}

MSXRom16KB::~MSXRom16KB()
{
	delete [] memoryBank; // C++ can handle null-pointers
	PRT_DEBUG("Destructing an MSXRom16KB object");
}

void MSXRom16KB::init()
{
	MSXDevice::init();
	
	// allocate buffer
	memoryBank=new byte[ROM_SIZE];
	if (memoryBank == NULL)
		PRT_ERROR("Couldn't create 16KB rom bank !!!!!!");
	
	// read the rom file
	std::string filename=deviceConfig->getParameter("romfile");
	int offset = atoi(deviceConfig->getParameter("skip_headerbytes").c_str());
	
#ifdef HAVE_FSTREAM_TEMPL
	std::ifstream<byte> file(filename.c_str());
#else
	std::ifstream file(filename.c_str());
#endif
	file.seekg(offset);
	file.read(memoryBank, ROM_SIZE);
	if (file.fail())
		PRT_ERROR("Error reading " << filename);
	
	registerSlots();
}

byte MSXRom16KB::readMem(word address, Emutime &time)
{
	return memoryBank [address & 0x3fff];
}

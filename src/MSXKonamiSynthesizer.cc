// $Id$

#include "MSXKonamiSynthesizer.hh" 
#include "MSXMotherBoard.hh"
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"

MSXKonamiSynthesizer::MSXKonamiSynthesizer(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXKonamiSynthesizer object");
}

MSXKonamiSynthesizer::~MSXKonamiSynthesizer()
{
	delete DAC; // C++ can handle null-pointers
	delete [] memoryBank; // C++ can handle null-pointers
	PRT_DEBUG("Destructing an MSXKonamiSynthesizer object");
}

void MSXKonamiSynthesizer::init()
{
	MSXDevice::init();
	DAC = new DACSound(25000);	// TODO find a good value, put it into config file
	
	// allocate buffer
	memoryBank=new byte[ROM_SIZE + 0x4000 ];
	if (memoryBank == NULL)
		PRT_ERROR("Couldn't create 32KB rom bank !!!!!!");
	
	// read the rom file
	std::string filename=deviceConfig->getParameter("romfile");
	int offset = atoi(deviceConfig->getParameter("skip_headerbytes").c_str());
	
#ifdef HAVE_FSTREAM_TEMPL
	std::ifstream<byte> file(filename.c_str());
#else
	std::ifstream file(filename.c_str());
#endif
	file.seekg(offset);
	file.read(memoryBank + 0x4000, ROM_SIZE);
	if (file.fail())
		PRT_ERROR("Error reading " << filename);
	
	registerSlots();
}

byte MSXKonamiSynthesizer::readMem(word address, Emutime &time)
{
	return memoryBank [address];
}

void MSXKonamiSynthesizer::writeMem(word address, byte value, Emutime &time)
{
	// Should be only for 0x4000, but since this isn't confirmed on a real cratridge we will simply
	// use every write.
	DAC->writeDAC(value,time);
}

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
	PRT_DEBUG("Destructing an MSXKonamiSynthesizer object");
	delete DAC;
	delete [] memoryBank;
}

void MSXKonamiSynthesizer::init()
{
	MSXDevice::init();
	DAC = new DACSound(25000);	// TODO find a good value, put it into config file
	loadFile(&memoryBank, 0x8000);
	registerSlots();
}

byte MSXKonamiSynthesizer::readMem(word address, Emutime &time)
{
	return memoryBank [address-0x4000];
}

void MSXKonamiSynthesizer::writeMem(word address, byte value, Emutime &time)
{
	// Should be only for 0x4000, but since this isn't confirmed on a real
	// cratridge we will simply use every write.
	DAC->writeDAC(value,time);
}

// $Id$

#include "MSXRom16KB.hh" 
#include "MSXMotherBoard.hh"
#include <string>
#include <iostream>
#include <fstream>


MSXRom16KB::MSXRom16KB()
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
	
	//std::ifstream<byte> file(filename.c_str());
	std::ifstream file(filename.c_str());
	file.seekg(offset);
	file.read(memoryBank, ROM_SIZE);
	if (file.fail())
		PRT_ERROR("Error reading " << filename);
	
	// register in slot-structure
	std::list<MSXConfig::Device::Slotted*>::const_iterator i;
	for (i=deviceConfig->slotted.begin(); i!=deviceConfig->slotted.end(); i++) {
		int ps=(*i)->getPS();
		int ss=(*i)->getSS();
		int page=(*i)->getPage();
		MSXMotherBoard::instance()->registerSlottedDevice(this,ps,ss,page);
	}
}

byte MSXRom16KB::readMem(word address, Emutime &time)
{
	return memoryBank [address & 0x3fff];
}

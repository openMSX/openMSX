// $Id$

#include "MSXRom16KB.hh" 
#include "MSXMotherBoard.hh"
#include <string>
#include <stdio.h>
#include <stdlib.h>

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
	// TODO: kill C way of file I/O and replace char* by string
	MSXDevice::init();
	
	FILE *file;
	const char *filename;
	const char *nrbytes;
	int ps;
	int ss;
	int page;
	memoryBank=new byte[16384];
	if (memoryBank == NULL) {
		PRT_ERROR("Couldn't create 16KB rom bank !!!!!!");
	} else {
		//Read the rom file
		// from [ANSI/ISO]: the default value of non-class objects is indeterminate
		memset(memoryBank,0,16384);
		filename=deviceConfig->getParameter("romfile").c_str();
		nrbytes=deviceConfig->getParameter("skip_headerbytes").c_str();
		file = fopen(filename,"r");
		if (file) {
			PRT_DEBUG("reading " << filename);
			fseek(file,atol(nrbytes),SEEK_SET);
			fread(memoryBank,16384,1,file);
		} else {
			PRT_ERROR("Error reading " << filename);
		}
		list<MSXConfig::Device::Slotted*>::const_iterator i;
		for (i=deviceConfig->slotted.begin(); i!=deviceConfig->slotted.end(); i++) {
			// Registering device in slot structure
			ps=(*i)->getPS();
			ss=(*i)->getSS();
			page=(*i)->getPage();
			MSXMotherBoard::instance()->registerSlottedDevice(this,ps,ss,page);
		}
	}
}

byte MSXRom16KB::readMem(word address, Emutime &time)
{
	return memoryBank[ address & 0x3fff] ;
};

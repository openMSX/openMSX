// $Id$

//
// Empty , just created to have a device for the factory and a general file for new developers
//
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXRom16KB.hh" 
#include "string.h"
#include "stdio.h"
//#include "stdlib.h"

MSXRom16KB::MSXRom16KB()
{
	cout << "Creating an MSXRom16KB object \n";
}

MSXRom16KB::~MSXRom16KB()
{
	delete [] memoryBank; // C++ can handle null-pointers
	cout << "Destructing an MSXRom16KB object \n";
}

void MSXRom16KB::init()
{
	FILE *file;
	memoryBank=new byte[16384];
	if (memoryBank == NULL){
		cout << "Couldn't create 16KB rom bank !!!!!!";
		//TODO: stop openMSX !!!!
	} else {
		//Read the rom file
		memset(memoryBank,0,16384); // TODO: Possible default of C++ look-up
		file = fopen(romfile,"r");
		if (file){
			fread(memoryBank,16384,1,file);
		} else {
		cout << "Error reading " << romfile << "\n";
		}

	}
}

byte MSXRom16KB::readMem(word address,UINT64 TStates)
{
	return memoryBank[ address] ;
};

//
// Empty , just created to have a device for the factory and a general file for new developers
//
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXSimple64KB.hh" 
#include "string.h"
//#include "stdlib.h"

MSXSimple64KB::MSXSimple64KB()
{
	cout << "Creating an MSXSimple64KB object \n";
}

MSXSimple64KB::~MSXSimple64KB()
{
	delete [] memoryBank; // C++ can handle null-pointers
	cout << "Destructing an MSXSimple64KB object \n";
}

void MSXSimple64KB::init()
{
	memoryBank=new byte[65536];
	if (memoryBank == NULL){
		cout << "Couldn't create 64KB memory bank !!!!!!";
		//TODO: stop openMSX !!!!
	} else {
		//Isn't completely true, but let's suppose that ram will 
		//always contain all zero if started
		memset(memoryBank,0,65536); // TODO: Possible default of C++ look-up
	}
	// MSXMotherBoard.register_IO_In((byte)81,this);
	// MSXMotherBoard.register_IO_Out((byte)80,this);
}

byte MSXSimple64KB::readMem(word address,UINT64 TStates)
{
	return memoryBank[ address] ;
};

void MSXSimple64KB::writeMem(word address,byte value,UINT64 TStates)
{
	//TODO: Actually one should test memoryBank before assigning
	// or could be avoided by implementing previous TODO nr. 1 :-)
	memoryBank[address]=value;
};    

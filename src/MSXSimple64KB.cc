// $Id$

//
// Empty , just created to have a device for the factory and a general file for new developers
//
#include "MSXSimple64KB.hh" 
#include "string.h"
//#include "stdlib.h"

MSXSimple64KB::MSXSimple64KB()
{
	cout << "Creating an MSXSimple64KB object \n";
	slow_drain_on_reset=false;
}

MSXSimple64KB::~MSXSimple64KB()
{
	delete [] memoryBank; // C++ can handle null-pointers
	cout << "Destructing an MSXSimple64KB object \n";
}

void MSXSimple64KB::start()
{
	cout << "Starting an MSXSimple64KB object \n";
}

void MSXSimple64KB::reset()
{
	cout << "Reseting an MSXSimple64KB object \n";
	if (!slow_drain_on_reset ){
		cout << "Clearing ram of MSXSimple64KB object \n";
		memset(memoryBank,0,65536);
	}
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

	//MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,0);
	//MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,1);
	MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,2);
	MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,3);

}

byte MSXSimple64KB::readMem(word address, Emutime &time)
{
	return memoryBank[ address] ;
};

void MSXSimple64KB::writeMem(word address, byte value, Emutime &time)
{
	//TODO: Actually one should test memoryBank before assigning
	// or could be avoided by implementing previous TODO nr. 1 :-)
	memoryBank[address]=value;
};    

// $Id$

//
// Empty , just created to have a device for the factory and a general file for new developers
//
#include "MSXSimple64KB.hh" 
#include "string.h"
//#include "stdlib.h"

MSXSimple64KB::MSXSimple64KB()
{
	PRT_DEBUG("Creating an MSXSimple64KB object \n");
	slow_drain_on_reset=false;
}

MSXSimple64KB::~MSXSimple64KB()
{
	delete [] memoryBank; // C++ can handle null-pointers
	PRT_DEBUG("Destructing an MSXSimple64KB object \n");
}

void MSXSimple64KB::start()
{
	PRT_DEBUG("Starting an MSXSimple64KB object \n");
}

void MSXSimple64KB::reset()
{
	PRT_DEBUG("Reseting an MSXSimple64KB object \n");
	if (!slow_drain_on_reset ) {
		PRT_DEBUG("Clearing ram of MSXSimple64KB object \n");
		memset(memoryBank,0,65536);
	}
}

void MSXSimple64KB::init()
{
	memoryBank=new byte[65536];
	if (memoryBank == NULL){
		PRT_ERROR("Couldn't create 64KB memory bank !!!!!!");
	} else {
		//Isn't completely true, but let's suppose that ram will 
		//always contain all zero if started
		memset(memoryBank,0,65536); // TODO: Possible default of C++ look-up
	}
	// MSXMotherBoard.register_IO_In((byte)81,this);
	// MSXMotherBoard.register_IO_Out((byte)80,this);

	list<MSXConfig::Device::Slotted*> slotted_list = deviceConfig->slotted;
	for (list<MSXConfig::Device::Slotted*>::const_iterator i=slotted_list.begin(); i != slotted_list.end(); i++)
	{
	  MSXMotherBoard::instance()->registerSlottedDevice(this,
	      (*i)->getPS(),
	      (*i)->getSS(),
	      (*i)->getPage() );
	}

	//MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,0);
	//MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,1);
	//MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,2);
	//MSXMotherBoard::instance()->registerSlottedDevice(this,0,0,3);

}

byte MSXSimple64KB::readMem(word address, Emutime &time)
{
	return memoryBank[ address] ;
};

void MSXSimple64KB::writeMem(word address, byte value, Emutime &time)
{
	memoryBank[address]=value;
};    

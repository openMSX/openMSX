// $Id$

//
// Empty , just created to have a device for the factory and a general file for new developers
//
#ifndef __MSXROM16KB_HH__
#define __MSXROM16KB_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"

class MSXRom16KB : public MSXDevice
{
	private:
		byte* memoryBank;	
	public:
		//constructor and destructor
		MSXRom16KB();
		~MSXRom16KB();
		
		// don't forget you inherited from MSXDevice
		void init();
		//void reset();
		//void StopMSX();
		//void RestoreMSX();
		//void SaveStateMSX(ofstream savestream);
		//
		byte readMem(word address, Emutime &time);
//		char  romfile[255]; //temporary public
};
#endif

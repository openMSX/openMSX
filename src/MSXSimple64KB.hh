// $Id$

//
// Empty , just created to have a device for the factory and a general file for new developers
//
#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"

class MSXSimple64KB : public MSXDevice
{
	private:
		byte* memoryBank;	
		bool slow_drain_on_reset;
	public:
		//constructor and destructor
		MSXSimple64KB();
		~MSXSimple64KB();
		
		// don't forget you inherited from MSXDevice
		void init();
		void start();
		void reset();
		//void reset();
		//void SaveStateMSX(ofstream savestream);
		//
		byte readMem(word address,UINT64 TStates);
                void writeMem(word address,byte value,UINT64 TStates);  
};
#endif

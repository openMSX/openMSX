// $Id$

#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"

class MSXSimple64KB : public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXSimple64KB(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXSimple64KB();
		
		// don't forget you inherited from MSXDevice
		void init();
		void start();
		void reset();
		
		//void SaveStateMSX(ofstream savestream);
		
		byte readMem(word address, Emutime &time);
		void writeMem(word address, byte value, Emutime &time);  
	
	private:
		byte* memoryBank;
		bool slowDrainOnReset;
};
#endif

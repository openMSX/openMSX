// $Id$

#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include "MSXMemDevice.hh"
#include "MSXMotherBoard.hh"
#include "EmuTime.hh"

class MSXSimple64KB : public MSXMemDevice
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
		
		void init();
		void reset(const EmuTime &time);
		
		//void SaveStateMSX(ofstream savestream);
		
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);  
	
	private:
		byte* memoryBank;
		bool slowDrainOnReset;
};
#endif

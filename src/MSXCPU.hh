// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "MSXDevice.hh"
#include "CPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "CPU.hh"
#include "Z80.hh"
//#include "MSXR800.hh"

class MSXCPU : public MSXDevice, public CPUInterface 
{
	public:
		enum CPUType { CPU_Z80 };	//, CPU_R800 };
	
		virtual ~MSXCPU();
		static MSXCPU *instance();
	
		// MSXDevice
		void init();
		void reset();
		
		// CPUInterface
		bool IRQStatus();
		byte readIO(word port, Emutime &time);
		void writeIO (word port, byte value, Emutime &time);
		byte readMem(word address, Emutime &time);
		void writeMem(word address, byte value, Emutime &time);

		// MSXCPU
		void executeUntilTarget(const Emutime &time);
		void setTargetTime(const Emutime &time);
		const Emutime &getCurrentTime();
		
		void setActiveCPU(CPUType cpu);
		const Emutime &getTargetTime();

	private:
		MSXCPU();
		static MSXCPU *oneInstance;
		void executeUntilEmuTime(const Emutime &time); // prevent use

		Z80 *z80;
		//MSXR800 *r800;
		CPU *activeCPU;

		MSXMotherBoard *mb;
};
#endif //__MSXCPU_HH__

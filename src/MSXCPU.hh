// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "MSXDevice.hh"
#include "emutime.hh"
#include "MSXCPUDevice.hh"
#include "MSXZ80.hh"
//#include "MSXR800.hh"
class MSXZ80;
//class MSXR800;

class MSXCPU : public MSXDevice 
{
	public:
		enum CPUType { Z80 };	//, R800 };
	
		virtual ~MSXCPU();
		static MSXCPU *instance();
		
		void init();
		void reset();
		void setTargetTime(const Emutime &time);
		void executeUntilTarget(const Emutime &time);
		
		void setActiveCPU(CPUType cpu);
		Emutime &getCurrentTime();
		const Emutime &getTargetTime();

	private:
		MSXCPU();
		static MSXCPU *oneInstance;
		void executeUntilEmuTime(const Emutime &time); // prevent use

		MSXZ80 *z80;
		//MSXR800 *r800;
		MSXCPUDevice *activeCPU;

		Emutime targetTime;
};
#endif //__MSXCPU_HH__


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
	friend class MSXZ80;
	//friend class MSXR800;

	public:
		virtual ~MSXCPU();
		static MSXCPU *instance();
		
		void init();
		void reset();
		void IRQ(bool irq);
		void setTargetTime(const Emutime &time);
		void executeUntilTarget(const Emutime &time);
		
		void setActiveCPU(int cpu);
		Emutime &getCurrentTime();
		const Emutime &getTargetTime();

	private:
		static const int CPU_Z80 = 0;
		static const int CPU_R800 = 1;
	
		MSXCPU();
		static MSXCPU *oneInstance;
		void executeUntilEmuTime(const Emutime &time); // prevent use

		MSXZ80 *z80;
		//MSXR800 *r800;
		MSXCPUDevice *activeCPU;

		Emutime targetTime;
};
#endif //__MSXCPU_HH__

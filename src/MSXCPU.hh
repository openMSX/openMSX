
#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "MSXDevice.hh"
#include "emutime.hh"
#include "MSXCPUDevice.hh"
#include "MSXZ80.hh"
//#include "MSXR800.hh"


class MSXCPU : public MSXDevice
{
	public:
		~MSXCPU();
		static MSXCPU *instance();
		
		void init();
		void reset();
		
		void setActiveCPU(int cpu);
		
		void IRQ(bool irq);
		void executeUntilEmuTime(const Emutime &time);

	private:
		static const int CPU_Z80 = 0;
		static const int CPU_R800 = 1;
	
		MSXCPU();
		static MSXCPU *oneInstance;

		MSXZ80 *z80;
		//MSXR800 *r800;
		MSXCPUDevice *activeCPU;
};
#endif //__MSXCPU_HH__

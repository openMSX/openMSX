// $Id$

#ifndef __MSXZ80_HH__
#define __MSXZ80_HH__

#include "emutime.hh"
#include "Z80.hh"
#include "MSXCPUDevice.hh"

class MSXZ80 : public MSXCPUDevice
{
friend void Interrupt(int i);
friend int Z80_SingleInstruction(void);
friend void Z80_Out (byte Port,byte Value);
friend byte Z80_RDMEM(word A);
friend byte Z80_In (byte Port);
friend void Z80_WRMEM(word A,byte V);

	public:
		// constructor and destructor
		MSXZ80();
		virtual ~MSXZ80();
		
		void init();
		void reset();
		void IRQ(bool irq);
		void executeUntilEmuTime(const Emutime &time);

	private:
		void setTargetTStates(const Emutime &time);
		
		static Emutime currentCPUTime; // static -> temporary hack for c/c++ mix
		Emutime targetCPUTime;
};
#endif //__MSXZ80_H__

// $Id$

#ifndef __MSXZ80_HH__
#define __MSXZ80_HH__

#include "emutime.hh"
#include "Z80.hh"
#include "MSXCPUDevice.hh"
#include "MSXCPU.hh"
class MSXCPU;

class MSXZ80 : public MSXCPUDevice
{
friend void Interrupt(int i);
friend int Z80_SingleInstruction(void);

	public:
		// constructor and destructor
		MSXZ80();
		virtual ~MSXZ80();
		
		//MSXCPUDevice
		void init();
		void reset();
		void IRQ(bool irq);
		void executeUntilTarget();
};
#endif //__MSXZ80_HH__

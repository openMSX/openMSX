// $Id$
//
// MSXRom Patch hook: an object with a method that is called
// when EDFE is encountered by the CPU, for a certain address
//

#ifndef __MSXROMPATCHINTERFACE_HH__
#define __MSXROMPATCHINTERFACE_HH__

#include "CPU.hh"


class MSXRomPatchInterface
{
	public:
		/**
		 * called by CPU on encountering an EDFE
		 */
		virtual void patch(CPU::CPURegs& regs) const = 0;
};

#endif // __MSXROMPATCHINTERFACE_HH__

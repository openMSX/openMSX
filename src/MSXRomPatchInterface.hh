// $Id$
//
// MSXRom Patch hook: an object with a method that is called
// when EDFE is encountered by the CPU, for a certain address
//

#ifndef __MSXROMPATCHINTERFACE_HH__
#define __MSXROMPATCHINTERFACE_HH__

#include "CPU.hh"


namespace openmsx {

class MSXRomPatchInterface
{
public:
	virtual ~MSXRomPatchInterface() {}
	
	/**
	 * called by CPU on encountering an EDFE
	 */
	virtual void patch(CPU::CPURegs& regs) = 0;
};

} // namespace openmsx

#endif // __MSXROMPATCHINTERFACE_HH__

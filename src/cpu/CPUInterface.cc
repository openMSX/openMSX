// $Id$

#include "CPUInterface.hh"
#include "MSXRomPatchInterface.hh"

CPUInterface::CPUInterface()
{
	prevNMIStat = NMIStatus();
}

CPUInterface::~CPUInterface()
{
}

inline bool CPUInterface::NMIStatus()
{
	return false;
}

bool CPUInterface::NMIEdge()
{
	bool newNMIStat = NMIStatus();
	bool result = newNMIStat && !prevNMIStat;
	prevNMIStat = newNMIStat;
	return result;
}

byte CPUInterface::dataBus()
{
	return 255;
}

void CPUInterface::patch(CPU::CPURegs& regs)
{
	// walk all interfaces, it's up to the interface
	// to decide to do anything
	for (list<MSXRomPatchInterface*>::const_iterator i =
		romPatchInterfaceList.begin();
	     i != romPatchInterfaceList.end();
	     ++i) {
		(*i)->patch(regs);
	}
}

void CPUInterface::registerInterface(MSXRomPatchInterface *i)
{
	romPatchInterfaceList.push_back(i);
}
void CPUInterface::unregisterInterface(MSXRomPatchInterface *i)
{
	romPatchInterfaceList.remove(i);
}

void CPUInterface::reti(CPU::CPURegs& regs)
{
}

void CPUInterface::retn(CPU::CPURegs& regs)
{
}


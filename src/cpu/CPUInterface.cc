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
	std::list<const MSXRomPatchInterface*>::const_iterator i;
	for (i=romPatchInterfaceList.begin(); i!=romPatchInterfaceList.end(); i++) {
		(*i)->patch(regs);
	}
}

void CPUInterface::registerInterface(const MSXRomPatchInterface *i)
{
	romPatchInterfaceList.push_back(i);
}
void CPUInterface::unregisterInterface(const MSXRomPatchInterface *i)
{
	romPatchInterfaceList.remove(i);
}

void CPUInterface::reti(CPU::CPURegs& regs)
{
}

void CPUInterface::retn(CPU::CPURegs& regs)
{
}


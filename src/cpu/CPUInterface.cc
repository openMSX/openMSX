// $Id$

#include "CPUInterface.hh"
#include "MSXRomPatchInterface.hh"
#include <algorithm>

namespace openmsx {

CPUInterface::CPUInterface()
{
	prevNMIStat = NMIStatus();
}

CPUInterface::~CPUInterface()
{
}

bool CPUInterface::NMIStatus()
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
	for (RomPatches::const_iterator it = romPatches.begin();
	     it != romPatches.end(); ++it) {
		(*it)->patch(regs);
	}
}

void CPUInterface::registerInterface(MSXRomPatchInterface* i)
{
	romPatches.push_back(i);
}
void CPUInterface::unregisterInterface(MSXRomPatchInterface* i)
{
	romPatches.erase(std::remove(romPatches.begin(), romPatches.end(), i),
	                 romPatches.end());
}

void CPUInterface::reti(CPU::CPURegs& /*regs*/)
{
}

void CPUInterface::retn(CPU::CPURegs& /*regs*/)
{
}

} // namespace openmsx


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

bool CPUInterface::NMIStatus()
{
	return false;
}

bool CPUInterface::NMIEdge()
{
	bool newNMIStat = NMIStatus();
	bool result = !prevNMIStat && newNMIStat;
	prevNMIStat = newNMIStat;
	return result;
}

byte CPUInterface::dataBus()
{
	return 255;
}

void CPUInterface::patch()
{
	PRT_DEBUG("void CPUInterface::patch ()");
	// for now it just walks all interfaces
	// it's up to the interface to decide to do anything
	std::list<const MSXRomPatchInterface*>::const_iterator i =
		romPatchInterfaceList.begin();
	for ( /**/ ; i != romPatchInterfaceList.end() ; i++)
	{
		(*i)->patch();
	}
}

void CPUInterface::registerInterface(const MSXRomPatchInterface *i)
{
	romPatchInterfaceList.push_back(i);
	PRT_DEBUG("Registered MSXDiskRomPatch at CPUInterface");
}

void CPUInterface::reti ()
{
}

void CPUInterface::retn ()
{
}

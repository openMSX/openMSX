// $Id$

#include "CPUInterface.hh"

CPUInterface::CPUInterface()
{
	prevNMIStat = NMIStatus();
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

void CPUInterface::patch () {}
void CPUInterface::reti () {}
void CPUInterface::retn () {}


// $Id$
//

#include "MSXRom.hh"
#include "MSXDiskRomPatch.hh"

MSXRom::MSXRom()
{
	handleRomPatchInterfaces();
}

MSXRom::~MSXRom()
{
	PRT_DEBUG("Deleting a MSXRom memoryBank");
	delete [] memoryBank;
	std::list<MSXRomPatchInterface*>::iterator i =
		romPatchInterfaces.begin();
	for ( /**/ ; i!= romPatchInterfaces.end(); i++)
	{
		PRT_DEBUG("Deleting a RomPatchInterface for an MSXRom");
		delete (*i);
	}
}

MSXConfig::Device* MSXRom::getDeviceConfig()
{
	return deviceConfig;
}

void MSXRom::handleRomPatchInterfaces()
{
	// for each patchcode parameter, construct apropriate patch
	// object and register it at 
	std::list<const MSXConfig::Config::Parameter*> parameters =
		deviceConfig->getParametersWithClass("patchcode");
	std::list<const MSXConfig::Config::Parameter*>::const_iterator i =
		parameters.begin();
	for ( /**/ ; i!=parameters.end(); i++)
	{
		if ((*i)->name == "MSXDiskRomPatch")
		{
			PRT_DEBUG("Creating MSXDiskRomPatch");
			MSXRomPatchInterface* i=new MSXDiskRomPatch();
			romPatchInterfaces.push_back(i);
			PRT_DEBUG("Registering MSXDiskRomPatch at CPU [TODO]");
			// TODO XXX
		}
	}
}

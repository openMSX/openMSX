// $Id$
//

#include "MSXRom.hh"

MSXRom::MSXRom()
{
	handleRomPatchInterfaces();
}

MSXRom::~MSXRom()
{
	PRT_DEBUG("Deleting a MSXRom memoryBank");
	delete [] memoryBank;
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
	}
}

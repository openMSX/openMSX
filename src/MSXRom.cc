// $Id$
//

#include "MSXRom.hh"

MSXRom::~MSXRom()
{
    PRT_DEBUG("Deleting a MSXRom memoryBank");
	delete [] memoryBank;
}

MSXConfig::Device* MSXRom::getDeviceConfig()
{
	return deviceConfig;
}

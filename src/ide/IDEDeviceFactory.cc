// $Id$

#include "IDEDeviceFactory.hh"
#include "MSXConfig.hh"
#include "IDEHD.hh"


IDEDevice* IDEDeviceFactory::create(const std::string &name,
                                    const EmuTime &time)
{
	MSXConfig::Config *config = MSXConfig::Backend::instance()->
		getConfigById(name);
	const std::string type = config->getType();

	if (type == "IDEHD") {
		return new IDEHD(config, time);
	}
	PRT_ERROR("Unknown IDE device: " << type);
	return NULL;
}

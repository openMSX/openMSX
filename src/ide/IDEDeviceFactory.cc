// $Id$

#include "IDEDeviceFactory.hh"
#include "MSXConfig.hh"
#include "IDEHD.hh"


namespace openmsx {

IDEDevice* IDEDeviceFactory::create(const string &name,
                                    const EmuTime &time)
{
	Config *config = MSXConfig::instance()->
		getConfigById(name);
	const string& type = config->getType();

	if (type == "IDEHD") {
		return new IDEHD(config, time);
	}
	PRT_ERROR("Unknown IDE device: " << type);
	return NULL;
}

} // namespace openmsx

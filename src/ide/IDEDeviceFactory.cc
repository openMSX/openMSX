// $Id$

#include "IDEDeviceFactory.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "IDEHD.hh"


namespace openmsx {

IDEDevice* IDEDeviceFactory::create(const string& name,
                                    const EmuTime& time)
{
	Config* config = MSXConfig::instance()->getConfigById(name);
	const string& type = config->getType();

	if (type == "IDEHD") {
		return new IDEHD(config, time);
	}
	throw FatalError("Unknown IDE device: " + type);
}

} // namespace openmsx

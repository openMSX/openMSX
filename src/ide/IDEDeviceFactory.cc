// $Id$

#include "IDEDeviceFactory.hh"
#include "HardwareConfig.hh"
#include "ConfigException.hh"
#include "IDEHD.hh"

namespace openmsx {

IDEDevice* IDEDeviceFactory::create(const string& name,
                                    const EmuTime& time)
{
	const XMLElement* config = HardwareConfig::instance().findConfigById(name);
	if (!config) {
		throw ConfigException("Configuration for IDE device " + name +
		                      " missing.");
	}
	
	const string& type = config->getChildData("type");
	if (type == "IDEHD") {
		return new IDEHD(*config, time);
	}
	
	throw FatalError("Unknown IDE device: " + type);
}

} // namespace openmsx

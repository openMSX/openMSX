// $Id$

#include "IDEDeviceFactory.hh"
#include "IDEHD.hh"
#include "xmlx.hh"

using std::string;

namespace openmsx {

IDEDevice* IDEDeviceFactory::create(const XMLElement& config,
                                    const EmuTime& time)
{
	const string& type = config.getChildData("type");
	if (type == "IDEHD") {
		return new IDEHD(config, time);
	}
	
	throw FatalError("Unknown IDE device: " + type);
}

} // namespace openmsx

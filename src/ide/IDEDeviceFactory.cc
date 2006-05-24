// $Id$

#include "IDEDeviceFactory.hh"
#include "IDEHD.hh"
#include "IDECDROM.hh"
#include "XMLElement.hh"
#include "MSXException.hh"

using std::string;

namespace openmsx {

IDEDevice* IDEDeviceFactory::create(
	CommandController& commandController,
	EventDistributor& eventDistributor,
	const XMLElement& config,
	const EmuTime& time
	)
{
	const string& type = config.getChildData("type");
	if (type == "IDEHD") {
		return new IDEHD(eventDistributor, config, time);
	} else if (type == "IDECDROM") {
		return new IDECDROM(commandController, eventDistributor, config, time);
	}

	throw MSXException("Unknown IDE device: " + type);
}

} // namespace openmsx

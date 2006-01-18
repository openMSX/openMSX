// $Id$

#include "ExtensionConfig.hh"

namespace openmsx {

ExtensionConfig::ExtensionConfig(MSXMotherBoard& motherBoard)
	: HardwareConfig(motherBoard)
{
}

const XMLElement& ExtensionConfig::getDevices() const
{
	return getConfig();
}

} // namespace openmsx

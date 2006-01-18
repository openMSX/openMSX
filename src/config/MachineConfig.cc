// $Id$

#include "MachineConfig.hh"
#include "XMLElement.hh"

namespace openmsx {

MachineConfig::MachineConfig(MSXMotherBoard& motherBoard)
	: HardwareConfig(motherBoard)
{
}

const XMLElement& MachineConfig::getDevices() const
{
	return getConfig().getChild("devices");
}

} // namespace openmsx

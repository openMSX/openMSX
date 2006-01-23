// $Id$

#include "MachineConfig.hh"
#include "XMLElement.hh"

namespace openmsx {

MachineConfig::MachineConfig(MSXMotherBoard& motherBoard,
                             const std::string& machineName)
	: HardwareConfig(motherBoard)
{
	load("machines", machineName);
}

const XMLElement& MachineConfig::getDevices() const
{
	return getConfig().getChild("devices");
}

} // namespace openmsx

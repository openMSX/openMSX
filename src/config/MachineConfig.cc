// $Id$

#include "MachineConfig.hh"
#include "XMLElement.hh"

namespace openmsx {

MachineConfig::MachineConfig(MSXMotherBoard& motherBoard,
                             const std::string& machineName)
	: HardwareConfig(motherBoard, machineName)
{
	load("machines");
}

} // namespace openmsx

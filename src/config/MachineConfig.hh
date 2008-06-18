// $Id$

#ifndef MACHINECONFIG_HH
#define MACHINECONFIG_HH

#include "HardwareConfig.hh"

namespace openmsx {

class MachineConfig : public HardwareConfig
{
public:
	MachineConfig(MSXMotherBoard& motherBoard,
	              const std::string& machineName);
};

} // namespace openmsx

#endif

// $Id$

#ifndef MACHINECONFIG_HH
#define MACHINECONFIG_HH

#include "HardwareConfig.hh"

namespace openmsx {

class MachineConfig : public HardwareConfig
{
public:
	MachineConfig(MSXMotherBoard& motherBoard);
	
	virtual const XMLElement& getDevices() const;
};

} // namespace openmsx

#endif

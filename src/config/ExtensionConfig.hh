// $Id$

#ifndef EXTENSIONCONFIG_HH
#define EXTENSIONCONFIG_HH

#include "HardwareConfig.hh"
#include <memory>

namespace openmsx {

class ExtensionConfig : public HardwareConfig
{
public:
	ExtensionConfig(MSXMotherBoard& motherBoard);
	
	virtual const XMLElement& getDevices() const;
};

} // namespace openmsx

#endif

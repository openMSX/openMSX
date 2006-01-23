// $Id$

#ifndef EXTENSIONCONFIG_HH
#define EXTENSIONCONFIG_HH

#include "HardwareConfig.hh"

namespace openmsx {

class ExtensionConfig : public HardwareConfig
{
public:
	ExtensionConfig(MSXMotherBoard& motherBoard,
	                const std::string& name);
	ExtensionConfig(MSXMotherBoard& motherBoard,
	                const std::string& romfile, const std::string& slotname,
	                const std::vector<std::string>& options);

	virtual const XMLElement& getDevices() const;
};

} // namespace openmsx

#endif

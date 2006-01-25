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

	const std::string& getName() const;

	virtual const XMLElement& getDevices() const;

private:
	void setName(const std::string& proposedName);

	std::string name;
};

} // namespace openmsx

#endif

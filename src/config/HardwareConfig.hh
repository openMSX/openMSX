// $Id$

#ifndef __HARDWARECONFIG_HH__
#define __HARDWARECONFIG_HH__

#include "MSXConfig.hh"

namespace openmsx {

class HardwareConfig : public MSXConfig
{
public:
	static HardwareConfig& instance();

	static void loadHardware(XMLElement& root, const std::string& path,
	                         const std::string& hwName);

private:
	HardwareConfig();
	~HardwareConfig();
};

} // namespace openmsx

#endif

// $Id$

#ifndef __HARDWARECONFIG_HH__
#define __HARDWARECONFIG_HH__

#include "MSXConfig.hh"

namespace openmsx {

class HardwareConfig : public MSXConfig
{
public:
	static HardwareConfig& instance();

	static void loadHardware(XMLElement& root, const string& path,
	                         const string& hwName);

private:
	HardwareConfig();
	~HardwareConfig();
};

} // namespace openmsx

#endif

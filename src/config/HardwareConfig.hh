// $Id$

#ifndef __HARDWARECONFIG_HH__
#define __HARDWARECONFIG_HH__

#include "MSXConfig.hh"

namespace openmsx {

class HardwareConfig : public MSXConfig
{
public:
	static HardwareConfig& instance();

	static void loadHardware(XMLElement& root, FileContext& context,
	                         const string& filename);

private:
	HardwareConfig();
	~HardwareConfig();
};

} // namespace openmsx

#endif

// $Id$

#ifndef __HARDWARECONFIG_HH__
#define __HARDWARECONFIG_HH__

#include "MSXConfig.hh"
#include "FileException.hh"

namespace openmsx {

class HardwareConfig : public MSXConfig
{
public:
	static HardwareConfig& instance();

	void loadHardware(FileContext& context, const string &filename)
		throw(FileException, ConfigException);

private:
	HardwareConfig();
	~HardwareConfig();
};

} // namespace openmsx

#endif

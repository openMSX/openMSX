// $Id$

#ifndef __SETTINGSCONFIG_HH__
#define __SETTINGSCONFIG_HH__

#include "MSXConfig.hh"

namespace openmsx {

class SettingsConfig : public MSXConfig
{
public:
	static SettingsConfig& instance();

	void loadSetting(FileContext& context, const string& filename);

private:
	SettingsConfig();
	~SettingsConfig();
};

} // namespace openmsx

#endif

// $Id$

#ifndef __GLOBALSETTINGS_HH__
#define __GLOBALSETTINGS_HH__

#include "BooleanSetting.hh"

namespace openmsx {

/**
 * This class contains settings that are used by several other class
 * (including some singletons). This class was introduced to solve
 * lifetime management issues.
 */
class GlobalSettings
{
public:
	static GlobalSettings& instance();

	BooleanSetting& getPauseSetting();
	BooleanSetting& getPowerSetting();
	BooleanSetting& getAutoSaveSetting();

private:
	GlobalSettings();
	
	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
	BooleanSetting autoSaveSetting;
};

} // namespace openmsx

#endif

// $Id$

#ifndef __GLOBALSETTINGS_HH__
#define __GLOBALSETTINGS_HH__

#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include <memory.h>

using std::auto_ptr;

namespace openmsx {

class XMLElement;

/**
 * This class contains settings that are used by several other class
 * (including some singletons). This class was introduced to solve
 * lifetime management issues.
 */
class GlobalSettings
{
public:
	static GlobalSettings& instance();

	IntegerSetting& getSpeedSetting();
	BooleanSetting& getThrottleSetting();
	BooleanSetting& getPauseSetting();
	BooleanSetting& getPowerSetting();
	BooleanSetting& getAutoSaveSetting();
	BooleanSetting& getConsoleSetting();
	StringSetting& getUserDirSetting();
	XMLElement& getMediaConfig();

private:
	GlobalSettings();
	
	IntegerSetting speedSetting;
	BooleanSetting throttleSetting;
	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
	BooleanSetting autoSaveSetting;
	BooleanSetting consoleSetting;
	StringSetting userDirSetting;
	auto_ptr<XMLElement> mediaConfig;
};

} // namespace openmsx

#endif

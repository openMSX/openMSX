// $Id$

#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include <memory>

namespace openmsx {

class XMLElement;
class IntegerSetting;
class BooleanSetting;
class StringSetting;

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
	StringSetting&  getUserDirSetting();
	XMLElement& getMediaConfig();

private:
	GlobalSettings();
	~GlobalSettings();

	std::auto_ptr<IntegerSetting> speedSetting;
	std::auto_ptr<BooleanSetting> throttleSetting;
	std::auto_ptr<BooleanSetting> pauseSetting;
	std::auto_ptr<BooleanSetting> powerSetting;
	std::auto_ptr<BooleanSetting> autoSaveSetting;
	std::auto_ptr<BooleanSetting> consoleSetting;
	std::auto_ptr<StringSetting>  userDirSetting;
	std::auto_ptr<XMLElement> mediaConfig;
};

} // namespace openmsx

#endif

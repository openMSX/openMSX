// $Id$

#include "BooleanSetting.hh"
#include "SettingsManager.hh"

namespace openmsx {

BooleanSetting::BooleanSetting(const string& name, const string& description,
                               bool initialValue)
	: EnumSettingBase<bool>(name, description, initialValue, getMap())
{
	SettingsManager::instance().registerSetting(*this);
}

BooleanSetting::~BooleanSetting()
{
	SettingsManager::instance().unregisterSetting(*this);
}

const EnumSetting<bool>::Map& BooleanSetting::getMap()
{
	static EnumSetting<bool>::Map boolMap;
	static bool alreadyInit = false;

	if (!alreadyInit) {
		alreadyInit = true;
		boolMap["on"]  = true;
		boolMap["off"] = false;
	}
	return boolMap;
}

} // namespace openmsx

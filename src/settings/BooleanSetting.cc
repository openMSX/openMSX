// $Id$

#include "BooleanSetting.hh"

namespace openmsx {

BooleanSetting::BooleanSetting(const string& name, const string& description,
                               bool initialValue, XMLElement* node)
	: EnumSettingBase<bool>(name, description, initialValue, getMap(), node)
{
	initSetting();
}

BooleanSetting::~BooleanSetting()
{
	exitSetting();
}

const EnumSetting<bool>::Map& BooleanSetting::getMap()
{
	static EnumSetting<bool>::Map boolMap;
	static bool alreadyInit = false;

	if (!alreadyInit) {
		alreadyInit = true;
		boolMap["on"]  = true;
		boolMap["off"] = false;
		boolMap["true"]  = true;
		boolMap["false"] = false;
		boolMap["yes"]  = true;
		boolMap["no"] = false;
	}
	return boolMap;
}

} // namespace openmsx

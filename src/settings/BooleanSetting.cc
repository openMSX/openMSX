// $Id$

#include "BooleanSetting.hh"

namespace openmsx {

BooleanSetting::BooleanSetting(const string& name, const string& description,
                               bool initialValue)
	: EnumSettingBase<bool>(name, description, initialValue, getMap())
{
	initSetting();
}

BooleanSetting::BooleanSetting(XMLElement& node, const string& description)
	: EnumSettingBase<bool>(node, description, node.getDataAsBool(), getMap())
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

// $Id$

#include "BooleanSetting.hh"

using std::string;

namespace openmsx {

BooleanSetting::BooleanSetting(const string& name, const string& description,
                               bool initialValue, SaveSetting save)
	: EnumSetting<bool>(name, description, initialValue, getMap(), save)
{
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

// $Id$

#include "BooleanSetting.hh"

namespace openmsx {

BooleanSetting::BooleanSetting(
	const string &name, const string &description,
	bool initialValue)
	: EnumSetting<bool>(name, description, initialValue, getMap())
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
	}
	return boolMap;
}

} // namespace openmsx

// $Id$

#include "BooleanSetting.hh"


namespace openmsx {

BooleanSetting::BooleanSetting(
	const string &name, const string &description,
	bool initialValue)
	: EnumSetting<bool>(name, description, initialValue, getMap())
{
}

const map<string, bool> &BooleanSetting::getMap()
{
	static map<string, bool> boolMap;
	static bool alreadyInit = false;

	if (!alreadyInit) {
		alreadyInit = true;
		boolMap["on"]  = true;
		boolMap["off"] = false;
	}
	return boolMap;
}

} // namespace openmsx

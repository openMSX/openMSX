// $Id$

#include "IntegerSetting.hh"
#include "CommandException.hh"
#include <sstream>
#include <cstdio>

namespace openmsx {

// class IntegerSettingPolicy

IntegerSettingPolicy::IntegerSettingPolicy(int minValue, int maxValue)
	: SettingRangePolicy<int>(minValue, maxValue)
{
}

std::string IntegerSettingPolicy::toString(int value) const
{
	std::ostringstream out;
	out << value;
	return out.str();
}

int IntegerSettingPolicy::fromString(const std::string& str) const
{
	char* endPtr;
	int result = strtol(str.c_str(), &endPtr, 0);
	if (*endPtr != '\0') {
		throw CommandException("not a valid integer: " + str);
	}
	return result;
}


// class IntegerSetting

IntegerSetting::IntegerSetting(const string& name, const string& description,
                               int initialValue, int minValue, int maxValue)
	: SettingImpl<IntegerSettingPolicy>(name, description, initialValue,
	                                    Setting::SAVE, minValue, maxValue)
{
}

} // namespace openmsx

#include "IntegerSetting.hh"
#include "CommandException.hh"
#include <sstream>
#include <cstdio>

using std::string;

namespace openmsx {

// class IntegerSettingPolicy

IntegerSettingPolicy::IntegerSettingPolicy(int minValue, int maxValue)
	: SettingRangePolicy<int>(minValue, maxValue)
{
}

string IntegerSettingPolicy::toString(int value) const
{
	std::ostringstream out;
	out << value;
	return out.str();
}

int IntegerSettingPolicy::fromString(const string& str) const
{
	char* endPtr;
	int result = strtol(str.c_str(), &endPtr, 0);
	if (*endPtr != '\0') {
		throw CommandException("not a valid integer: " + str);
	}
	return result;
}

string_ref IntegerSettingPolicy::getTypeString() const
{
	return "integer";
}


// class IntegerSetting

IntegerSetting::IntegerSetting(CommandController& commandController,
                               string_ref name, string_ref description,
                               int initialValue, int minValue, int maxValue)
	: SettingImpl<IntegerSettingPolicy>(
		commandController, name, description, initialValue,
		Setting::SAVE, minValue, maxValue)
{
}

} // namespace openmsx

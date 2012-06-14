// $Id$

#include "FloatSetting.hh"
#include "CommandException.hh"
#include <sstream>
#include <iomanip>
#include <cstdio>

using std::string;

namespace openmsx {

// class FloatSettingPolicy

FloatSettingPolicy::FloatSettingPolicy(double minValue, double maxValue)
	: SettingRangePolicy<double>(minValue, maxValue)
{
}

string FloatSettingPolicy::toString(double value) const
{
	std::stringstream out;
	out << std::setprecision(2) << std::fixed << std::showpoint << value;
	return out.str();
}

double FloatSettingPolicy::fromString(const string& str) const
{
	char* endPtr;
	double result = strtod(str.c_str(), &endPtr);
	if (*endPtr != '\0') {
		throw CommandException("not a valid float: " + str);
	}
	return result;
}

string_ref FloatSettingPolicy::getTypeString() const
{
	return "float";
}


// class FloatSetting

FloatSetting::FloatSetting(CommandController& commandController,
                           string_ref name, string_ref description,
                           double initialValue, double minValue, double maxValue)
	: SettingImpl<FloatSettingPolicy>(
		commandController, name, description, initialValue,
		Setting::SAVE, minValue, maxValue)
{
}

} // namespace openmsx

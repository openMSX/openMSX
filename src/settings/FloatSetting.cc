// $Id$

#include "FloatSetting.hh"
#include "CommandException.hh"
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace openmsx {

// class FloatSettingPolicy

FloatSettingPolicy::FloatSettingPolicy(double minValue, double maxValue)
	: SettingRangePolicy<double>(minValue, maxValue)
{
}

std::string FloatSettingPolicy::toString(double value) const
{
	std::stringstream out;
	out << std::setprecision(2) << std::fixed << std::showpoint << value;
	return out.str();
}

double FloatSettingPolicy::fromString(const std::string& str) const
{
	char* endPtr;
	double result = strtod(str.c_str(), &endPtr);
	if (*endPtr != '\0') {
		throw CommandException("not a valid float: " + str);
	}
	return result;
}


//class FloatSetting

FloatSetting::FloatSetting(const string& name, const string& description,
                           double initialValue, double minValue, double maxValue)
	: SettingImpl<FloatSettingPolicy>(name, description, initialValue,
	                                  Setting::SAVE, minValue, maxValue)
{
}

} // namespace openmsx

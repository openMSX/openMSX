// $Id$

#include "FloatSetting.hh"
#include "CommandException.hh"
#include "SettingsManager.hh"
#include <sstream>
#include <iomanip>
#include <cstdio>

using std::ostringstream;


namespace openmsx {

FloatSetting::FloatSetting(
	const string& name, const string& description,
	float initialValue, float minValue, float maxValue)
	: Setting<float>(name, description, initialValue)
{
	setRange(minValue, maxValue);
	SettingsManager::instance().registerSetting(*this);
}

FloatSetting::~FloatSetting()
{
	SettingsManager::instance().unregisterSetting(*this);
}

void FloatSetting::setRange(float minValue, float maxValue)
{
	this->minValue = minValue;
	this->maxValue = maxValue;

	// Update the setting type to the new range.
	char rangeStr[12];
	snprintf(rangeStr, 12, "%.2f - %.2f", minValue, maxValue);
	type = string(rangeStr);
	/* The following C++ style code doesn't work on GCC 2.95:
	ostringstream out;
	out << setprecision(2) << fixed << showpoint
		<< minValue << " - " << maxValue;
	type = out.str();
	*/

	// Clip to new range.
	setValue(getValue());
}

string FloatSetting::getValueString() const
{
	char rangeStr[5];
	snprintf(rangeStr, 5, "%.2f", getValue());
	return string(rangeStr);
	/* The following C++ style code doesn't work on GCC 2.95:
	ostringstream out;
	out << setprecision(2) << fixed << showpoint
		<< value;
	return out.str();
	*/
}

void FloatSetting::setValueString(const string &valueString)
	throw(CommandException)
{
	float newValue;
	int converted = sscanf(valueString.c_str(), "%f", &newValue);
	if (converted != 1) {
		throw CommandException(
			"set: " + valueString + ": not a valid float");
	}
	setValue(newValue);
}

void FloatSetting::setValue(const float &newValue)
{
	// TODO: Almost identical copy of IntegerSetting::setValue.
	//       A definate sign Ranged/OrdinalSetting is a good idea.
	float nv = newValue;
	if (nv < minValue) {
		nv = minValue;
	} else if (nv > maxValue) {
		nv = maxValue;
	}
	Setting<float>::setValue(nv);
}

} // namespace openmsx

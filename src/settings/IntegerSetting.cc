// $Id$

#include "IntegerSetting.hh"
#include "CommandException.hh"
#include <sstream>
#include <cstdio>

using std::ostringstream;


namespace openmsx {

IntegerSetting::IntegerSetting(const string& name, const string& description,
                               int initialValue, int minValue, int maxValue)
	: Setting<int>(name, description, initialValue)
{
	setRange(minValue, maxValue);
	initSetting(SAVE_SETTING);
}

IntegerSetting::~IntegerSetting()
{
	exitSetting();
}

void IntegerSetting::setRange(int minValue, int maxValue)
{
	this->minValue = minValue;
	this->maxValue = maxValue;

	// Update the setting type to the new range.
	ostringstream out;
	out << minValue << " - " << maxValue;
	type = out.str();

	// Clip to new range.
	setValue(getValue());
}

string IntegerSetting::getValueString() const
{
	ostringstream out;
	out << getValue();
	return out.str();
}

void IntegerSetting::setValueString(const string &valueString)
{
	char* endPtr;
	int newValue = strtol(valueString.c_str(), &endPtr, 0);
	if (*endPtr != '\0') {
		throw CommandException(
			"set: " + valueString + ": not a valid integer");
	}
	setValue(newValue);
}

void IntegerSetting::setValue(const int& newValue)
{
	int nv = newValue;
	if (nv < minValue) {
		nv = minValue;
	} else if (nv > maxValue) {
		nv = maxValue;
	}
	Setting<int>::setValue(nv);
}

} // namespace openmsx

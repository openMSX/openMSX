// $Id$

#include "Settings.hh"
#include "File.hh"
#include "FileContext.hh"
#include "CliCommunicator.hh"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdio>

using std::ostringstream;


namespace openmsx {

// Setting implementation:
// Located in .hh to avoid template instantiation problems.

// IntegerSetting implementation:

IntegerSetting::IntegerSetting(
	const string &name_, const string &description_,
	int initialValue, int minValue_, int maxValue_)
	: Setting<int>(name_, description_, initialValue)
{
	setRange(minValue_, maxValue_);
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
	char *endPtr;
	long newValue = strtol(valueString.c_str(), &endPtr, 0);
	if (*endPtr != '\0') {
		throw CommandException(
			"set: " + valueString + ": not a valid integer");
	}
	setValue(newValue);
}

void IntegerSetting::setValue(const int &newValue)
{
	int nv = newValue;
	if (nv < minValue) {
		nv = minValue;
	} else if (nv > maxValue) {
		nv = maxValue;
	}
	Setting<int>::setValue(nv);
}

// FloatSetting implementation:

FloatSetting::FloatSetting(
	const string &name_, const string &description_,
	float initialValue, float minValue_, float maxValue_)
	: Setting<float>(name_, description_, initialValue)
{
	setRange(minValue_, maxValue_);
}

void FloatSetting::setRange(const float minValue, const float maxValue)
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

// IntStringMap implementation:

IntStringMap::IntStringMap(BaseMap *map_)
{
	this->stringToInt = map_;
}

IntStringMap::~IntStringMap()
{
	delete stringToInt;
}

const string &IntStringMap::lookupInt(int n) const
{
	for (MapIterator it = stringToInt->begin()
	; it != stringToInt->end() ; ++it) {
		if (it->second == n) return it->first;
	}
	throw MSXException("Integer not in map: " + n);
}

int IntStringMap::lookupString(const string &s) const
{
	MapIterator it = stringToInt->find(s);
	if (it != stringToInt->end()) return it->second;
	// TODO: Don't use the knowledge that we're called inside command
	//       processing: use a different exception.
	throw CommandException("set: " + s + ": not a valid value");
}

string IntStringMap::getSummary() const
{
	ostringstream out;
	MapIterator it = stringToInt->begin();
	out << it->first;
	for (++it; it != stringToInt->end(); ++it) {
		out << ", " << it->first;
	}
	return out.str();
}

set<string> *IntStringMap::createStringSet() const
{
	set<string> *ret = new set<string>;
	for (MapIterator it = stringToInt->begin()
	; it != stringToInt->end(); ++it) {
		ret->insert(it->first);
	}
	return ret;
}

// BooleanSetting implementation

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


// StringSetting implementation

StringSetting::StringSetting(
	const string &name_, const string &description_,
	const string &initialValue)
	: Setting<string>(name_, description_, initialValue)
{
}

string StringSetting::getValueString() const
{
	return getValue();
}

void StringSetting::setValueString(const string &newValue)
{
	setValue(newValue);
}


// FilenameSetting implementation

FilenameSetting::FilenameSetting(const string &name,
	const string &description,
	const string &initialValue)
	: StringSetting(name, description, initialValue)
{
}

void FilenameSetting::setValue(const string &newValue)
{
	string resolved;
	try {
		UserFileContext context;
		resolved = context.resolve(newValue);
	} catch (FileException &e) {
		// File not found.
		CliCommunicator::instance().printWarning(
			"couldn't find file: \"" + newValue + "\"");
		return;
	}
	if (checkFile(resolved)) {
		StringSetting::setValue(newValue);
	}
}

void FilenameSetting::tabCompletion(vector<string> &tokens) const
{
	CommandController::completeFileName(tokens);
}

bool FilenameSetting::checkFile(const string &filename)
{
	return true;
}

} // namespace openmsx

// $Id$

#include "Settings.hh"
#include "CommandController.hh"
#include "RenderSettings.hh"
#include "OSDConsoleRenderer.hh"
#include "File.hh"
#include "FileContext.hh"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <stdio.h>

using std::ostringstream;


// Setting implementation:

Setting::Setting(const string &name_, const string &description_)
	: name(name_), description(description_)
{
	SettingsManager::instance()->registerSetting(name, this);
}

Setting::~Setting()
{
	SettingsManager::instance()->unregisterSetting(name);
}


// IntegerSetting implementation:

IntegerSetting::IntegerSetting(
	const string &name_, const string &description_,
	int initialValue, int minValue_, int maxValue_)
	: Setting(name_, description_), value(initialValue)
{
	setRange(minValue_, maxValue_);
}

void IntegerSetting::setRange(const int minValue, const int maxValue)
{
	this->minValue = minValue;
	this->maxValue = maxValue;

	// update the setting type to the new range
	ostringstream out;
	out << minValue << " - " << maxValue;
	type = out.str();

	if (value < minValue || value > maxValue) setValueInt(value);
}

string IntegerSetting::getValueString() const
{
	ostringstream out;
	out << value;
	return out.str();
}

void IntegerSetting::setValueString(const string &valueString)
{
	char *endPtr;
	long newValue = strtol(valueString.c_str(), &endPtr, 0);
	if (*endPtr != '\0') {
		throw CommandException(
			"Not a valid integer: \"" + valueString + "\"");
	}
	setValueInt(newValue);
}

void IntegerSetting::setValueInt(int newValue)
{
	if (newValue < minValue) {
		newValue = minValue;
	} else if (newValue > maxValue) {
		newValue = maxValue;
	}
	if (checkUpdate(newValue)) {
		value = newValue;
	}
}

// FloatSetting implementation:

FloatSetting::FloatSetting(
	const string &name_, const string &description_,
	float initialValue, float minValue_, float maxValue_)
	: Setting(name_, description_), value(initialValue)
{
	setRange(minValue_, maxValue_);
}

void FloatSetting::setRange(const float minValue, const float maxValue)
{
	this->minValue = minValue;
	this->maxValue = maxValue;

	// update the setting type to the new range
	char rangeStr[12];
	snprintf(rangeStr, 12, "%.2f - %.2f", minValue, maxValue);
	type = string(rangeStr);
	/* The following C++ style code doesn't work on GCC 2.95:
	ostringstream out;
	out << std::setprecision(2) << std::fixed << std::showpoint
		<< minValue << " - " << maxValue;
	type = out.str();
	*/

	if (value < minValue || value > maxValue) setValueFloat(value);
}

string FloatSetting::getValueString() const
{
	char rangeStr[5];
	snprintf(rangeStr, 5, "%.2f", value);
	return string(rangeStr);
	/* The following C++ style code doesn't work on GCC 2.95:
	ostringstream out;
	out << std::setprecision(2) << std::fixed << std::showpoint
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
			"Not a valid float: \"" + valueString + "\"");
	}
	setValueFloat(newValue);
}

void FloatSetting::setValueFloat(float newValue)
{
	if (newValue < minValue) {
		newValue = minValue;
	} else if (newValue > maxValue) {
		newValue = maxValue;
	}
	if (checkUpdate(newValue)) {
		value = newValue;
	}
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
	; it != stringToInt->end() ; it++) {
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
	throw CommandException("Not a valid value: \"" + s + "\"");
}

string IntStringMap::getSummary() const
{
	ostringstream out;
	MapIterator it = stringToInt->begin();
	out << it->first;
	for (it++; it != stringToInt->end(); it++) {
		out << ", " << it->first;
	}
	return out.str();
}

set<string> *IntStringMap::createStringSet() const
{
	set<string> *ret = new set<string>;
	for (MapIterator it = stringToInt->begin()
	; it != stringToInt->end(); it++) {
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

const map<const string, bool> &BooleanSetting::getMap()
{
	static map<const string, bool> boolMap;
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
	: Setting(name_, description_), value(initialValue)
{
}

string StringSetting::getValueString() const
{
	return value;
}

void StringSetting::setValueString(const string &newValue)
{
	if (checkUpdate(newValue)) {
		value = newValue;
	}
}


// FilenameSetting implementation

FilenameSetting::FilenameSetting(const string &name,
	const string &description,
	const string &initialValue)
	: StringSetting(name, description, initialValue)
{
}

void FilenameSetting::tabCompletion(vector<string> &tokens) const
{
	CommandController::completeFileName(tokens);
}


// SettingsManager implementation:

SettingsManager::SettingsManager()
	: setCommand(this)
	, toggleCommand(this)
{
	CommandController::instance()->registerCommand(&setCommand, "set");
	CommandController::instance()->registerCommand(&toggleCommand, "toggle");
}

SettingsManager::~SettingsManager()
{
	CommandController::instance()->unregisterCommand(&setCommand, "set");
	CommandController::instance()->unregisterCommand(&toggleCommand, "toggle");
}


// SetCommand implementation:

SettingsManager::SetCommand::SetCommand(SettingsManager *manager_)
	: manager(manager_)
{
}

void SettingsManager::SetCommand::execute(
	const vector<string> &tokens )
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 3) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		// List all settings.
		map<string, Setting *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); it++) {
			print(it->first);
		}
		return;
	}

	// Get setting object.
	const string &name = tokens[1];
	Setting *setting = manager->getByName(name);
	if (!setting) {
		throw CommandException("There is no setting named \""
		                       + name + "\"");
	}

	if (nrTokens == 2) {
		// Info.
		print(setting->getDescription());
		print("current value   : " + setting->getValueString());
		if (!setting->getTypeString().empty()) {
			print("possible values : " + setting->getTypeString());
		}
	} else {
		// Change.
		const string &valueString = tokens[2];
		setting->setValueString(valueString);
	}

}

void SettingsManager::SetCommand::help(
	const vector<string> &tokens) const
{
	print("set            : list all settings");
	print("set name       : information on setting");
	print("set name value : change setting's value");
}

void SettingsManager::SetCommand::tabCompletion(
	vector<string> &tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			map<string, Setting *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); it++) {
				settings.insert(it->first);
			}
			CommandController::completeString(tokens, settings);
			break;
		}
		case 3: {
			// complete setting value
			map<string, Setting*>::iterator it =
				manager->settingsMap.find(tokens[1]);
			if (it != manager->settingsMap.end()) {
				it->second->tabCompletion(tokens);
			}
			break;
		}
	}
}

// ToggleCommand implementation:

SettingsManager::ToggleCommand::ToggleCommand(SettingsManager *manager_)
	: manager(manager_)
{
}

void SettingsManager::ToggleCommand::execute(
	const vector<string> &tokens )
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 2) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		// list all boolean settings
		map<string, Setting *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); it++) {
			if (dynamic_cast<BooleanSetting*>(it->second)) {
				print(it->first);
			}
		}
		return;
	}
 
	// get setting object
	const string &name = tokens[1];
	Setting *setting = manager->getByName(name);
	if (!setting) {
		throw CommandException(
			"There is no setting named \"" + name + "\"" );
	}
	BooleanSetting *boolSetting = dynamic_cast<BooleanSetting*>(setting);
	if (!boolSetting) {
		throw CommandException(
			"The setting named \"" + name + "\" is not a boolean" );
	}
	// actual toggle
	boolSetting->setValue(!boolSetting->getValue());

}

void SettingsManager::ToggleCommand::help(
	const vector<string> &tokens) const
{
	print("toggle      : list all boolean settings");
	print("toggle name : toggles a boolean setting");
}

void SettingsManager::ToggleCommand::tabCompletion(
	vector<string> &tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			map<string, Setting *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); it++) {
				if (dynamic_cast<BooleanSetting*>(it->second)) {
					settings.insert(it->first);
				}
			}
			CommandController::completeString(tokens, settings);
			break;
		}
	}
}

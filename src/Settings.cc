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
using std::string;

// Force template instantiation:
template class EnumSetting<RenderSettings::Accuracy>;
template class EnumSetting<RendererFactory::RendererID>;
template class EnumSetting<OSDConsoleRenderer::Placement>;
template class EnumSetting<bool>;


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
	ostringstream out;
	out << std::setprecision(2) << std::fixed << std::showpoint
		<< minValue << " - " << maxValue;
	type = out.str();

	if (value < minValue || value > maxValue) setValueFloat(value);
}

string FloatSetting::getValueString() const
{
	ostringstream out;
	out << std::setprecision(2) << std::fixed << std::showpoint
		<< value;
	return out.str();
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

// EnumSetting implementation:

template <class T>
EnumSetting<T>::EnumSetting(
	const string &name_, const string &description_,
	const T &initialValue,
	const std::map<string, T> &map_)
	: Setting(name_, description_), value(initialValue), map(map_)
{
	ostringstream out;
	MapIterator it = map.begin();
	out << it->first;
	for (it++; it != map.end(); it++) {
		out << ", " << it->first;
	}
	type = out.str();
}

template <class T>
string EnumSetting<T>::getValueString() const
{
	MapIterator it = map.begin();
	while (it != map.end()) {
		if (it->second == value) {
			return it->first;
		}
		it++;
	}
	assert(false);
	return string("<unknown>");
}

template <class T>
void EnumSetting<T>::setValue(T newValue)
{
	if (checkUpdate(newValue)) {
		value = newValue;
	}
}

template <class T>
void EnumSetting<T>::setValueString(const string &valueString)
{
	MapIterator it = map.find(valueString);
	if (it != map.end()) {
		setValue(it->second);
	} else {
		throw CommandException(
			"Not a valid value: \"" + valueString + "\"");
	}
}

template <class T>
void EnumSetting<T>::tabCompletion(std::vector<string> &tokens) const
{
	std::set<string> values;
	for (MapIterator it = map.begin(); it != map.end(); it++) {
		values.insert(it->first);
	}
	CommandController::completeString(tokens, values);
}


// BooleanSetting implementation

BooleanSetting::BooleanSetting(
	const string &name, const string &description,
	bool initialValue)
	: EnumSetting<bool>(name, description, initialValue, getMap())
{
}

std::map<string, bool> &BooleanSetting::getMap()
{
	static std::map<string, bool> boolMap;
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

void FilenameSetting::tabCompletion(std::vector<string> &tokens) const
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
	const std::vector<string> &tokens )
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 3) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		// List all settings.
		std::map<string, Setting *>::const_iterator it =
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
	const std::vector<string> &tokens) const
{
	print("set            : list all settings");
	print("set name       : information on setting");
	print("set name value : change setting's value");
}

void SettingsManager::SetCommand::tabCompletion(
	std::vector<string> &tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			std::set<string> settings;
			std::map<string, Setting *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); it++) {
				settings.insert(it->first);
			}
			CommandController::completeString(tokens, settings);
			break;
		}
		case 3: {
			// complete setting value
			std::map<string, Setting*>::iterator it =
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
	const std::vector<string> &tokens )
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 2) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		// list all boolean settings
		std::map<string, Setting *>::const_iterator it =
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
	const std::vector<string> &tokens) const
{
	print("toggle      : list all boolean settings");
	print("toggle name : toggles a boolean setting");
}

void SettingsManager::ToggleCommand::tabCompletion(
	std::vector<string> &tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			std::set<string> settings;
			std::map<string, Setting *>::const_iterator it
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

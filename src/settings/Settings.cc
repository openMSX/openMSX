// $Id$

#include "Settings.hh"
//#include "CommandController.hh"
//#include "RenderSettings.hh"
//#include "OSDConsoleRenderer.hh"
#include "File.hh"
#include "FileContext.hh"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <stdio.h>

using std::ostringstream;

namespace openmsx {

// SettingNode implementation:

SettingNode::SettingNode(const string &name_, const string &description_)
	: name(name_), description(description_)
{
	SettingsManager::instance()->registerSetting(name, this);
}

SettingNode::~SettingNode()
{
	SettingsManager::instance()->unregisterSetting(name);
}

// SettingLeafNode implementation:

SettingLeafNode::SettingLeafNode(
	const string &name_, const string &description_)
	: SettingNode(name_, description_)
{
}

SettingLeafNode::~SettingLeafNode()
{
}

void SettingLeafNode::notify() const {
	list<SettingListener *>::const_iterator it;
	for (it = listeners.begin(); it != listeners.end(); ++it) {
		(*it)->update(this);
	}
}

void SettingLeafNode::addListener(SettingListener *listener) {
	listeners.push_back(listener);
}

void SettingLeafNode::removeListener(SettingListener *listener) {
	listeners.remove(listener);
}

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

void IntegerSetting::setRange(const int minValue, const int maxValue)
{
	this->minValue = minValue;
	this->maxValue = maxValue;

	// Update the setting type to the new range.
	ostringstream out;
	out << minValue << " - " << maxValue;
	type = out.str();

	// Clip to new range.
	setValue(value);
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
	setValue(value);
}

string FloatSetting::getValueString() const
{
	char rangeStr[5];
	snprintf(rangeStr, 5, "%.2f", value);
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
			"Not a valid float: \"" + valueString + "\"");
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
	throw CommandException("Not a valid value: \"" + s + "\"");
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
	return value;
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
		PRT_INFO("Warning: couldn't find file: \"" << newValue << "\"");
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
		map<string, SettingNode *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); ++it) {
			print(it->first);
		}
		return;
	}

	// Get setting object.
	// TODO: The cast is valid because currently all nodes are leaves.
	//       In the future this will no longer be the case.
	const string &name = tokens[1];
	SettingLeafNode *setting = static_cast<SettingLeafNode *>(
		manager->getByName(name)
		);
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
			map<string, SettingNode *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); ++it) {
				settings.insert(it->first);
			}
			CommandController::completeString(tokens, settings);
			break;
		}
		case 3: {
			// complete setting value
			map<string, SettingNode *>::iterator it =
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
		map<string, SettingNode *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); ++it) {
			if (dynamic_cast<BooleanSetting*>(it->second)) {
				print(it->first);
			}
		}
		return;
	}

	// get setting object
	const string &name = tokens[1];
	SettingNode *setting = manager->getByName(name);
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
			map<string, SettingNode *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); ++it) {
				if (dynamic_cast<BooleanSetting*>(it->second)) {
					settings.insert(it->first);
				}
			}
			CommandController::completeString(tokens, settings);
			break;
		}
	}
}

} // namespace openmsx

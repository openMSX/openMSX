// $Id$

#include <sstream>
#include <typeinfo>
#include "Settings.hh"
#include "CommandController.hh"
#include "RenderSettings.hh"
#include "File.hh"
#include "FileContext.hh"


// Force template instantiation
template class EnumSetting<RenderSettings::Accuracy>;
template class EnumSetting<bool>;


// Setting implementation:

Setting::Setting(const std::string &name_, const std::string &description_)
	: name(name_), description(description_)
{
	SettingsManager::instance()->registerSetting(name, this);
}

Setting::~Setting()
{
	SettingsManager::instance()->unregisterSetting(name);
}


// BooleanSetting implementation:

BooleanSetting::BooleanSetting(
	const std::string &name_, const std::string &description_,
	bool initialValue)
	: Setting(name_, description_), value(initialValue)
{
	type = "on - off";
}

std::string BooleanSetting::getValueString() const
{
	if (value) {
		return std::string("on");
	} else {
		return std::string("off");
	}
}

void BooleanSetting::setValue(bool newValue, const EmuTime &time)
{
	if (checkUpdate(newValue, time)) {
		value = newValue;
	}
}

void BooleanSetting::setValueString(const std::string &valueString,
                                    const EmuTime &time)
{
	bool newValue;
	if (valueString == "on") {
		newValue = true;
	} else if (valueString == "off") {
		newValue = false;
	} else {
		throw CommandException(
			"Not a valid boolean: \"" + valueString + "\"");
	}
	setValue(newValue, time);
}

void BooleanSetting::tabCompletion(std::vector<std::string> &tokens) const
{
	std::list<std::string> values;
	values.push_back("on");
	values.push_back("off");
	CommandController::completeString(tokens, values);
}


// IntegerSetting implementation:

IntegerSetting::IntegerSetting(
	const std::string &name_, const std::string &description_,
	int initialValue, int minValue_, int maxValue_)
	: Setting(name_, description_), value(initialValue),
	  minValue(minValue_), maxValue(maxValue_)
{
	std::ostringstream out;
	out << minValue << " - " << maxValue;
	type = out.str();
}

std::string IntegerSetting::getValueString() const
{
	std::ostringstream out;
	out << value;
	return out.str();
}

void IntegerSetting::setValueString(const std::string &valueString,
                                    const EmuTime &time)
{
	char *endPtr;
	long newValue = strtol(valueString.c_str(), &endPtr, 0);
	if (*endPtr != '\0') {
		throw CommandException(
			"Not a valid integer: \"" + valueString + "\"");
	}

	if (newValue < minValue) {
		newValue = minValue;
	} else if (newValue > maxValue) {
		newValue = maxValue;
	}
	if (checkUpdate((int)newValue, time)) {
		value = (int)newValue;
	}
}


// EnumSetting implementation:

template <class T>
EnumSetting<T>::EnumSetting(
	const std::string &name_, const std::string &description_,
	const T &initialValue,
	const std::map<const std::string, T> &map_)
	: Setting(name_, description_), value(initialValue), map(map_)
{
	std::ostringstream out;
	MapIterator it = map.begin();
	out << it->first;
	for (it++; it != map.end(); it++) {
		out << ", " << it->first;
	}
	type = out.str();
}

template <class T>
std::string EnumSetting<T>::getValueString() const
{
	MapIterator it = map.begin();
	while (it != map.end()) {
		if (it->second == value) {
			return it->first;
		}
		it++;
	}
	assert(false);
	return std::string("<unknown>");
}

template <class T>
void EnumSetting<T>::setValueString(const std::string &valueString,
                                    const EmuTime &time)
{
	MapIterator it = map.find(valueString);
	if (it != map.end()) {
		if (checkUpdate(it->second, time)) {
			value = it->second;
		}
	} else {
		throw CommandException(
			"Not a valid value: \"" + valueString + "\"");
	}
}

template <class T>
void EnumSetting<T>::tabCompletion(std::vector<std::string> &tokens) const
{
	std::list<std::string> values;
	for (MapIterator it = map.begin(); it != map.end(); it++) {
		values.push_back(it->first);
	}
	CommandController::completeString(tokens, values);
}


// StringSetting implementation

StringSetting::StringSetting(
	const std::string &name_, const std::string &description_,
	const std::string &initialValue)
	: Setting(name_, description_), value(initialValue)
{
}

std::string StringSetting::getValueString() const
{
	return value;
}

void StringSetting::setValueString(const std::string &newValue,
                                   const EmuTime &time)
{
	if (checkUpdate(newValue, time)) {
		value = newValue;
	}
}


// FilenameSetting implementation

FilenameSetting::FilenameSetting(const std::string &name,
                                 const std::string &description,
                                 const std::string &initialValue)
	: StringSetting(name, description, initialValue)
{
}

void FilenameSetting::tabCompletion(std::vector<std::string> &tokens) const
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
	const std::vector<std::string> &tokens, const EmuTime &time)
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 3) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		// List all settings.
		std::map<std::string, Setting *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); it++) {
			print(it->first);
		}
		return;
	}

	// Get setting object.
	const std::string &name = tokens[1];
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
		const std::string &valueString = tokens[2];
		setting->setValueString(valueString, time);
	}

}

void SettingsManager::SetCommand::help(
	const std::vector<std::string> &tokens) const
{
	print("set            : list all settings");
	print("set name       : information on setting");
	print("set name value : change setting's value");
}

void SettingsManager::SetCommand::tabCompletion(
	std::vector<std::string> &tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			std::list<std::string> settings;
			std::map<std::string, Setting *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); it++) {
				settings.push_back(it->first);
			}
			CommandController::completeString(tokens, settings);
			break;
		}
		case 3: {
			// complete setting value
			std::map<std::string, Setting*>::iterator it =
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
	const std::vector<std::string> &tokens, const EmuTime &time)
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 2) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		// List all boolean settings.
		std::map<std::string, Setting *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); it++) {
			if (typeid(*(it->second)) == typeid(BooleanSetting)) {
				print(it->first);
			}
		}
		return;
	}
 
	// Get setting object.
	const std::string &name = tokens[1];
	Setting *setting = manager->getByName(name);
	if (!setting) {
		throw CommandException(
			"There is no setting named \"" + name + "\"" );
	}
	if (typeid(*setting) != typeid(BooleanSetting)) {
		throw CommandException(
			"The setting named \"" + name + "\" is not a boolean" );
	}
	BooleanSetting *boolSetting = (BooleanSetting *)setting;

	// Actual toggle.
	boolSetting->setValue(!boolSetting->getValue(), time);

}

void SettingsManager::ToggleCommand::help(
	const std::vector<std::string> &tokens) const
{
	print("toggle      : list all boolean settings");
	print("toggle name : toggles a boolean setting");
}

void SettingsManager::ToggleCommand::tabCompletion(
	std::vector<std::string> &tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			std::list<std::string> settings;
			std::map<std::string, Setting *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); it++) {
				if (typeid(*(it->second)) == typeid(BooleanSetting)) {
					settings.push_back(it->first);
				}
			}
			CommandController::completeString(tokens, settings);
			break;
		}
	}
}

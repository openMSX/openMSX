// $Id$

#include <sstream>
#include "Settings.hh"
#include "CommandController.hh"
#include "RenderSettings.hh"
#include "OSDConsoleRenderer.hh"
#include "File.hh"
#include "FileContext.hh"


// Force template instantiation:
template class EnumSetting<RenderSettings::Accuracy>;
template class EnumSetting<RendererFactory::RendererID>;
template class EnumSetting<OSDConsoleRenderer::Placement>;
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

void IntegerSetting::setRange (const int minvalue,const int maxvalue)
{
	if (minvalue > maxvalue)
	{
		minValue = maxvalue; // autoswap if range is invalid
		maxValue = minvalue;
	}
	else
	{
		minValue = minvalue;
		maxValue = maxvalue;
	}
		
	int curValue=getValue();
	if (curValue < minValue) setValueInt(minValue);
	if (curValue > maxValue) setValueInt(maxValue);
	
	// update the setting type to the new range
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

void IntegerSetting::setValueString(const std::string &valueString)
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
	if (checkUpdate((int)newValue)) {
		value = (int)newValue;
	}	
}

// EnumSetting implementation:

template <class T>
EnumSetting<T>::EnumSetting(
	const std::string &name_, const std::string &description_,
	const T &initialValue,
	const std::map<std::string, T> &map_)
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
void EnumSetting<T>::setValue(T newValue)
{
	if (checkUpdate(newValue)) {
		value = newValue;
	}
}

template <class T>
void EnumSetting<T>::setValueString(const std::string &valueString)
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
void EnumSetting<T>::tabCompletion(std::vector<std::string> &tokens) const
{
	std::set<std::string> values;
	for (MapIterator it = map.begin(); it != map.end(); it++) {
		values.insert(it->first);
	}
	CommandController::completeString(tokens, values);
}


// BooleanSetting implementation

BooleanSetting::BooleanSetting(
	const std::string &name, const std::string &description,
	bool initialValue)
	: EnumSetting<bool>(name, description, initialValue, getMap())
{
}

std::map<std::string, bool> &BooleanSetting::getMap()
{
	static std::map<std::string, bool> boolMap;
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
	const std::string &name_, const std::string &description_,
	const std::string &initialValue)
	: Setting(name_, description_), value(initialValue)
{
}

std::string StringSetting::getValueString() const
{
	return value;
}

void StringSetting::setValueString(const std::string &newValue)
{
	if (checkUpdate(newValue)) {
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
	const std::vector<std::string> &tokens )
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
		setting->setValueString(valueString);
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
			std::set<std::string> settings;
			std::map<std::string, Setting *>::const_iterator it
				= manager->settingsMap.begin();
			for (; it != manager->settingsMap.end(); it++) {
				settings.insert(it->first);
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
	const std::vector<std::string> &tokens )
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 2) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		// list all boolean settings
		std::map<std::string, Setting *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); it++) {
			if (dynamic_cast<BooleanSetting*>(it->second)) {
				print(it->first);
			}
		}
		return;
	}
 
	// get setting object
	const std::string &name = tokens[1];
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
			std::set<std::string> settings;
			std::map<std::string, Setting *>::const_iterator it
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

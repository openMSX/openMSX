// $Id$

#include "Settings.hh"
#include "CommandController.hh"

#include <sstream>

// TODO: TEMP
#include <stdio.h>

// Setting implementation:

Setting::Setting(const std::string &name, const std::string &description)
{
	this->name = name;
	this->description = description;

	SettingsManager::getInstance()->registerSetting(name, this);
}

Setting::~Setting()
{
}

// IntegerSetting implementation:

IntegerSetting::IntegerSetting(
	const std::string &name, const std::string &description,
	int initialValue, int minValue, int maxValue )
	: Setting(name, description)
{
	value = initialValue;
	this->minValue = minValue;
	this->maxValue = maxValue;

	std::ostringstream out;
	out << minValue << " - " << maxValue;
	type = out.str();
}

std::string IntegerSetting::getValueString()
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
		throw new CommandException(
			"Not a valid integer: \"" + valueString + "\"" );
	}

	if (newValue < minValue) {
		newValue = minValue;
	} else if (newValue > maxValue) {
		newValue = maxValue;
	}
	value = (int)newValue;

	// TODO: Inform listeners.
}

// SettingsManager implementation:

SettingsManager::SettingsManager()
	: setCommand(this)
{
	CommandController::instance()->registerCommand(setCommand, "set");
}

// SetCommand implementation:

SettingsManager::SetCommand::SetCommand(
	SettingsManager *manager )
{
	this->manager = manager;
}

void SettingsManager::SetCommand::execute(
	const std::vector<std::string> &tokens )
{
	int nrTokens = tokens.size();
	if (nrTokens == 0 || nrTokens > 3) {
		throw CommandException("Wrong number of parameters");
	}

	if (nrTokens == 1) {
		std::map<std::string, Setting *>::const_iterator it =
			manager->settingsMap.begin();
		for (; it != manager->settingsMap.end(); it++) {
			print(it->first);
		}
		return;
	}

	const std::string &name = tokens[1];
	Setting *setting = manager->getByName(name);
	if (!setting) {
		throw CommandException("There is no setting named \"" + name + "\"" );
	}

	if (nrTokens == 2) {
		// Info.
		print(setting->getDescription());
		print("current value   : " + setting->getValueString());
		print("possible values : " + setting->getTypeString());
	} else {
		// Change.
		const std::string &valueString = tokens[2];
		setting->setValueString(valueString);
	}

}

void SettingsManager::SetCommand::help(
	const std::vector<std::string> &tokens )
{
	print("set            : list all settings");
	print("set name       : information on setting");
	print("set name value : change setting's value");
}


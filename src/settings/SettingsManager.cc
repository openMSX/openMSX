// $Id$

#include "SettingsManager.hh"
#include "Settings.hh"
#include "CommandController.hh"


namespace openmsx {

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
		throw CommandException("set: wrong number of parameters");
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
		throw CommandException("set: " + name + ": no such setting");
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
		throw CommandException("toggle: wrong number of parameters");
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
			"toggle: " + name + ": no such setting" );
	}
	BooleanSetting *boolSetting = dynamic_cast<BooleanSetting*>(setting);
	if (!boolSetting) {
		throw CommandException(
			"toggle: " + name + ": setting is not a boolean" );
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

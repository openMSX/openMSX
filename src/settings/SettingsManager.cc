// $Id$

#include <cstdlib>
#include "SettingsManager.hh"
#include "Settings.hh"
#include "CommandController.hh"


namespace openmsx {

// SettingsManager implementation:

SettingsManager::SettingsManager()
	: setCommand(this),
	  toggleCommand(this),
	  incrCommand(this),
	  decrCommand(this),
	  commandController(CommandController::instance())
{
	commandController.registerCommand(&setCommand,    "set");
	commandController.registerCommand(&toggleCommand, "toggle");
	commandController.registerCommand(&incrCommand,   "incr");
	commandController.registerCommand(&decrCommand,   "decr");
}

SettingsManager::~SettingsManager()
{
	commandController.unregisterCommand(&setCommand,    "set");
	commandController.unregisterCommand(&toggleCommand, "toggle");
	commandController.unregisterCommand(&incrCommand,   "incr");
	commandController.unregisterCommand(&decrCommand,   "decr");
}

SettingsManager& SettingsManager::instance()
{
	static SettingsManager oneInstance;
	return oneInstance;
}

// Helper functions for setting commands

template <typename T>
void SettingsManager::getSettingNames(string& result) const
{
	for (map<string, SettingNode*>::const_iterator it =
	       settingsMap.begin();
	     it != settingsMap.end(); ++it) {
		if (dynamic_cast<T*>(it->second)) {
			result += it->first + '\n';
		}
	}
}

template <typename T>
void SettingsManager::getSettingNames(set<string>& result) const
{
	for (map<string, SettingNode *>::const_iterator it
		 = settingsMap.begin();
	     it != settingsMap.end(); ++it) {
		if (dynamic_cast<T*>(it->second)) {
			result.insert(it->first);
		}
	}
}

template <typename T>
T* SettingsManager::getByName(const string& cmd, const string& name) const
{
	SettingNode* setting = getByName(name);
	if (!setting) {
		throw CommandException(cmd + ": " + name +
		                       ": no such setting");
	}
	T* typeSetting = dynamic_cast<T*>(setting);
	if (!typeSetting) {
		throw CommandException(cmd + ": " + name +
		                       ": setting has wrong type");
	}
	return typeSetting;
}


// SetCommand implementation:

SettingsManager::SetCommand::SetCommand(SettingsManager *manager_)
	: manager(manager_)
{
}

string SettingsManager::SetCommand::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	switch (tokens.size()) {
	case 1:
		// List all settings.
		manager->getSettingNames<SettingLeafNode>(result);
		break;
	
	case 2: {
		// Info.
		SettingLeafNode* setting = 
			manager->getByName<SettingLeafNode>("set", tokens[1]);
		result += setting->getDescription() + '\n';
		result += "current value   : " + setting->getValueString() + '\n';
		if (!setting->getTypeString().empty()) {
			result += "possible values : " + setting->getTypeString() + '\n';
		}
		break;
	}
	case 3: {
		// Change.
		SettingLeafNode* setting = 
			manager->getByName<SettingLeafNode>("set", tokens[1]);
		setting->setValueString(tokens[2]);
		break;
	}
	default:
		throw CommandException("set: wrong number of parameters");
	}
	return result;
}

string SettingsManager::SetCommand::help(const vector<string> &tokens) const
	throw()
{
	return "set            : list all settings\n"
	       "set name       : information on setting\n"
	       "set name value : change setting's value\n";
}

void SettingsManager::SetCommand::tabCompletion(vector<string> &tokens) const
	throw()
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			manager->getSettingNames<SettingLeafNode>(settings);
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

string SettingsManager::ToggleCommand::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	switch (tokens.size()) {
	case 1: 
		// list all boolean settings
		manager->getSettingNames<BooleanSetting>(result);
		break;

	case 2: {
		BooleanSetting *boolSetting =
			manager->getByName<BooleanSetting>("toggle", tokens[1]);
		boolSetting->setValue(!boolSetting->getValue());
		break;
	}
	default:
		throw CommandException("toggle: wrong number of parameters");
	}
	return result;
}

string SettingsManager::ToggleCommand::help(const vector<string> &tokens) const
	throw()
{
	return "toggle      : list all boolean settings\n"
	       "toggle name : toggles a boolean setting\n";
}

void SettingsManager::ToggleCommand::tabCompletion(vector<string> &tokens) const
	throw()
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			manager->getSettingNames<BooleanSetting>(settings);
			CommandController::completeString(tokens, settings);
			break;
		}
	}
}


// IncrCommand implementation:

SettingsManager::IncrCommand::IncrCommand(SettingsManager *manager_)
	: manager(manager_)
{
}

string SettingsManager::IncrCommand::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	int count = 1;
	switch (tokens.size()) {
	case 1:
		// list all integer settings
		manager->getSettingNames<IntegerSetting>(result);
		break;

	case 3:
		count = strtol(tokens[2].c_str(), NULL, 0);
		// fall-through
	case 2: {
		IntegerSetting *intSetting =
			manager->getByName<IntegerSetting>("incr", tokens[1]);
		intSetting->setValue(intSetting->getValue() + count);
		break;
	}
	default:
		throw CommandException("incr: wrong number of parameters");
	}
	return result;
}

string SettingsManager::IncrCommand::help(const vector<string> &tokens) const
	throw()
{
	return "incr            : list all integer settings\n"
	       "incr name       : increase the given integer setting\n"
	       "incr name count : decrease the given integer setting by count\n";
}

void SettingsManager::IncrCommand::tabCompletion(vector<string> &tokens) const
	throw()
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			manager->getSettingNames<IntegerSetting>(settings);
			CommandController::completeString(tokens, settings);
			break;
		}
	}
}


// DecrCommand implementation:

SettingsManager::DecrCommand::DecrCommand(SettingsManager *manager_)
	: manager(manager_)
{
}

string SettingsManager::DecrCommand::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	int count = 1;
	switch (tokens.size()) {
	case 1:
		// list all integer settings
		manager->getSettingNames<IntegerSetting>(result);
		break;

	case 3:
		count = strtol(tokens[2].c_str(), NULL, 0);
		// fall-through
	case 2: {
		IntegerSetting *intSetting =
			manager->getByName<IntegerSetting>("decr", tokens[1]);
		intSetting->setValue(intSetting->getValue() - count);
		break;
	}
	default:
		throw CommandException("decr: wrong number of parameters");
	}
	return result;
}

string SettingsManager::DecrCommand::help(const vector<string> &tokens) const
	throw()
{
	return "decr            : list all integer settings\n"
	       "decr name       : decrease the given integer setting\n"
	       "decr name count : decrease the given integer setting by count\n";
}

void SettingsManager::DecrCommand::tabCompletion(vector<string> &tokens) const
	throw()
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			manager->getSettingNames<IntegerSetting>(settings);
			CommandController::completeString(tokens, settings);
			break;
		}
	}
}

} // namespace openmsx

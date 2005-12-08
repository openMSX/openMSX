// $Id$

#include "SettingsManager.hh"
#include "CommandController.hh"
#include "Interpreter.hh"
#include "TclObject.hh"
#include "Setting.hh"
#include "CommandException.hh"
#include "XMLElement.hh"
#include <cassert>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

// SettingsManager implementation:

SettingsManager::SettingsManager(CommandController& commandController_)
	: settingInfo(commandController_, *this)
	, setCompleter(commandController_, *this)
	, incrCompleter(commandController_, *this, "incr")
	, unsetCompleter(commandController_, *this, "unset")
	, commandController(commandController_)
{
}

SettingsManager::~SettingsManager()
{
	assert(settingsMap.empty());
}

void SettingsManager::registerSetting(Setting& setting)
{
	const string& name = setting.getName();
	assert(settingsMap.find(name) == settingsMap.end());
	settingsMap[name] = &setting;

	commandController.getInterpreter().registerSetting(setting);
}

void SettingsManager::unregisterSetting(Setting& setting)
{
	commandController.getInterpreter().unregisterSetting(setting);

	const string& name = setting.getName();
	assert(settingsMap.find(name) != settingsMap.end());
	settingsMap.erase(name);
}

// Helper functions for setting commands

template <typename T>
void SettingsManager::getSettingNames(string& result) const
{
	for (SettingsMap::const_iterator it = settingsMap.begin();
	     it != settingsMap.end(); ++it) {
		if (dynamic_cast<T*>(it->second)) {
			result += it->first + '\n';
		}
	}
}

template <typename T>
void SettingsManager::getSettingNames(set<string>& result) const
{
	for (SettingsMap::const_iterator it = settingsMap.begin();
	     it != settingsMap.end(); ++it) {
		if (dynamic_cast<T*>(it->second)) {
			result.insert(it->first);
		}
	}
}

template <typename T>
T& SettingsManager::getByName(const string& cmd, const string& name) const
{
	Setting* setting = getByName(name);
	if (!setting) {
		throw CommandException(cmd + ": " + name +
		                       ": no such setting");
	}
	T* typeSetting = dynamic_cast<T*>(setting);
	if (!typeSetting) {
		throw CommandException(cmd + ": " + name +
		                       ": setting has wrong type");
	}
	return *typeSetting;
}

Setting* SettingsManager::getByName(const std::string& name) const
{
	SettingsMap::const_iterator it = settingsMap.find(name);
	// TODO: The cast is valid because currently all nodes are leaves.
	//       In the future this will no longer be the case.
	return it != settingsMap.end() ? it->second : NULL;
}

void SettingsManager::loadSettings(const XMLElement& config)
{
	// restore default values
	for (SettingsMap::const_iterator it = settingsMap.begin();
	     it != settingsMap.end(); ++it) {
		const Setting& setting = *it->second;
		if (setting.needLoadSave()) {
			it->second->restoreDefault();
		}
	}

	// load new values
	const XMLElement* settings = config.findChild("settings");
	if (!settings) return;
	for (SettingsMap::const_iterator it = settingsMap.begin();
	     it != settingsMap.end(); ++it) {
		const string& name = it->first;
		Setting& setting = *it->second;
		if (!setting.needLoadSave()) continue;
		const XMLElement* elem = settings->findChildWithAttribute(
			"setting", "id", name);
		if (elem) {
			try {
				setting.setValueString(elem->getData());
			} catch (MSXException& e) {
				// ignore, keep default value
			}
		}
	}
}

void SettingsManager::saveSettings(XMLElement& config) const
{
	for (SettingsMap::const_iterator it = settingsMap.begin();
	     it != settingsMap.end(); ++it) {
		it->second->sync(config);
	}
}

// SettingInfo implementation

SettingsManager::SettingInfo::SettingInfo(CommandController& commandController,
                                          SettingsManager& manager_)
	: InfoTopic(commandController, "setting")
	, manager(manager_)
{
}

void SettingsManager::SettingInfo::execute(
	const std::vector<TclObject*>& tokens, TclObject& result) const
{
	const SettingsMap& settingsMap = manager.settingsMap;
	switch (tokens.size()) {
	case 2:
		for (SettingsMap::const_iterator it = settingsMap.begin();
		     it != settingsMap.end(); ++it) {
			result.addListElement(it->first);
		}
		break;
	case 3: {
		string name = tokens[2]->getString();
		SettingsMap::const_iterator it = settingsMap.find(name);
		if (it == settingsMap.end()) {
			throw CommandException("No such setting: " + name);
		}
		it->second->info(result);
		break;
	}
	default:
		throw CommandException("Too many parameters.");
	}
}

string SettingsManager::SettingInfo::help(const vector<string>& /*tokens*/) const
{
	return "openmsx_info setting        : "
	             "returns list of all settings\n"
	       "openmsx_info setting <name> : "
	             "returns info on a specific setting\n";
}

void SettingsManager::SettingInfo::tabCompletion(
		std::vector<std::string>& tokens) const
{
	switch (tokens.size()) {
		case 3: { // complete setting name
			set<string> settings;
			manager.getSettingNames<Setting>(settings);
			completeString(tokens, settings);
			break;
		}
	}
}


// SetCompleter implementation:

SettingsManager::SetCompleter::SetCompleter(CommandController& commandController,
                                            SettingsManager& manager_)
	: CommandCompleter(commandController, "set"), manager(manager_)
{
}

string SettingsManager::SetCompleter::help(const vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		return manager.getByName<Setting>("set", tokens[1])
		                                          .getDescription();
	}
	return "Set or query the value of a openMSX setting or TCL variable\n"
	       "  set <setting>          shows current value\n"
	       "  set <setting> <value>  set a new value\n"
	       "Use 'help set <setting>' to get more info on a specific\n"
	       "openMSX setting.\n";
}

void SettingsManager::SetCompleter::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			manager.getSettingNames<Setting>(settings);
			completeString(tokens, settings);
			break;
		}
		case 3: {
			// complete setting value
			SettingsMap::iterator it =
				manager.settingsMap.find(tokens[1]);
			if (it != manager.settingsMap.end()) {
				it->second->tabCompletion(tokens);
			}
			break;
		}
	}
}


// SettingCompleter implementation

SettingsManager::SettingCompleter::SettingCompleter(
		CommandController& commandController, SettingsManager& manager_,
		const string& name)
	: CommandCompleter(commandController, name)
	, manager(manager_)
{
}

string SettingsManager::SettingCompleter::help(const vector<string>& /*tokens*/) const
{
	return ""; // TODO
}

void SettingsManager::SettingCompleter::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
		case 2: {
			// complete setting name
			set<string> settings;
			manager.getSettingNames<Setting>(settings);
			completeString(tokens, settings);
			break;
		}
	}
}

} // namespace openmsx

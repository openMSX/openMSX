// $Id$

#include "SettingsManager.hh"
#include "CommandController.hh"
#include "Interpreter.hh"
#include "Setting.hh"
#include "CommandException.hh"
#include "XMLElement.hh"

using std::set;
using std::string;
using std::vector;

namespace openmsx {

// SettingsManager implementation:

SettingsManager::SettingsManager()
	: setCompleter(*this)
	, settingCompleter(*this)
	, commandController(CommandController::instance())
{
	commandController.registerCompleter(&setCompleter,     "set");
	commandController.registerCompleter(&settingCompleter, "incr");
	commandController.registerCompleter(&settingCompleter, "unset");
}

SettingsManager::~SettingsManager()
{
	commandController.unregisterCompleter(&settingCompleter, "unset");
	commandController.unregisterCompleter(&settingCompleter, "incr");
	commandController.unregisterCompleter(&setCompleter,     "set");
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


// SetCompleter implementation:

SettingsManager::SetCompleter::SetCompleter(SettingsManager& manager_)
	: manager(manager_)
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
			CommandController::completeString(tokens, settings);
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

SettingsManager::SettingCompleter::SettingCompleter(SettingsManager& manager_)
	: manager(manager_)
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
			CommandController::completeString(tokens, settings);
			break;
		}
	}
}

} // namespace openmsx

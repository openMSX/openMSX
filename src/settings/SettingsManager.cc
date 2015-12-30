#include "SettingsManager.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "Setting.hh"
#include "CommandException.hh"
#include "XMLElement.hh"
#include "KeyRange.hh"
#include "outer.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

// SettingsManager implementation:

SettingsManager::SettingsManager(GlobalCommandController& commandController)
	: settingInfo   (commandController.getOpenMSXInfoCommand())
	, setCompleter  (commandController)
	, incrCompleter (commandController, *this, "incr")
	, unsetCompleter(commandController, *this, "unset")
{
}

SettingsManager::~SettingsManager()
{
	assert(settingsMap.empty());
}

void SettingsManager::registerSetting(BaseSetting& setting, string_ref name)
{
	assert(!settingsMap.contains(name));
	settingsMap.emplace_noDuplicateCheck(name.str(), &setting);
}

void SettingsManager::unregisterSetting(BaseSetting& /*setting*/, string_ref name)
{
	assert(settingsMap.contains(name));
	settingsMap.erase(name);
}

BaseSetting* SettingsManager::findSetting(string_ref name) const
{
	auto it = settingsMap.find(name);
	return (it != end(settingsMap)) ? it->second : nullptr;
}

// Helper functions for setting commands

BaseSetting& SettingsManager::getByName(string_ref cmd, string_ref name) const
{
	if (auto* setting = findSetting(name)) {
		return *setting;
	}
	throw CommandException(cmd + ": " + name + ": no such setting");
}

void SettingsManager::loadSettings(const XMLElement& config)
{
	// restore default values
	for (auto* s : values(settingsMap)) {
		if (s->needLoadSave()) {
			s->setValue(s->getRestoreValue());
		}
	}

	// load new values
	auto* settings = config.findChild("settings");
	if (!settings) return;
	for (auto& p : settingsMap) {
		auto& name = p.first;
		auto& setting = *p.second;
		if (!setting.needLoadSave()) continue;
		if (auto* elem = settings->findChildWithAttribute(
		                                     "setting", "id", name)) {
			try {
				setting.setValue(TclObject(elem->getData()));
			} catch (MSXException&) {
				// ignore, keep default value
			}
		}
	}
}


// class SettingInfo

SettingsManager::SettingInfo::SettingInfo(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "setting")
{
}

void SettingsManager::SettingInfo::execute(
	array_ref<TclObject> tokens, TclObject& result) const
{
	auto& manager = OUTER(SettingsManager, settingInfo);
	auto& settingsMap = manager.settingsMap;
	switch (tokens.size()) {
	case 2:
		for (auto& p : settingsMap) {
			result.addListElement(p.first);
		}
		break;
	case 3: {
		const auto& name = tokens[2].getString();
		auto it = settingsMap.find(name);
		if (it == end(settingsMap)) {
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

void SettingsManager::SettingInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		// complete setting name
		auto& manager = OUTER(SettingsManager, settingInfo);
		completeString(tokens, keys(manager.settingsMap));
	}
}


// class SetCompleter

SettingsManager::SetCompleter::SetCompleter(
		CommandController& commandController)
	: CommandCompleter(commandController, "set")
{
}

string SettingsManager::SetCompleter::help(const vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		auto& manager = OUTER(SettingsManager, setCompleter);
		return manager.getByName("set", tokens[1]).getDescription().str();
	}
	return "Set or query the value of a openMSX setting or Tcl variable\n"
	       "  set <setting>          shows current value\n"
	       "  set <setting> <value>  set a new value\n"
	       "Use 'help set <setting>' to get more info on a specific\n"
	       "openMSX setting.\n";
}

void SettingsManager::SetCompleter::tabCompletion(vector<string>& tokens) const
{
	auto& manager = OUTER(SettingsManager, setCompleter);
	switch (tokens.size()) {
	case 2:
		// complete setting name
		completeString(tokens, keys(manager.settingsMap), false); // case insensitive
		break;
	case 3: {
		// complete setting value
		auto it = manager.settingsMap.find(tokens[1]);
		if (it != end(manager.settingsMap)) {
			it->second->tabCompletion(tokens);
		}
		break;
	}
	}
}


// class SettingCompleter

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
	if (tokens.size() == 2) {
		// complete setting name
		completeString(tokens, keys(manager.settingsMap));
	}
}

} // namespace openmsx

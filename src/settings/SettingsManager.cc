#include "SettingsManager.hh"
#include "GlobalCommandController.hh"
#include "Command.hh"
#include "InfoTopic.hh"
#include "TclObject.hh"
#include "Setting.hh"
#include "CommandException.hh"
#include "XMLElement.hh"
#include "KeyRange.hh"
#include "memory.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

// SettingsManager implementation:

class SettingInfo : public InfoTopic
{
public:
	SettingInfo(InfoCommand& openMSXInfoCommand, SettingsManager& manager);
	virtual void execute(const vector<TclObject>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	SettingsManager& manager;
};

class SetCompleter : public CommandCompleter
{
public:
	SetCompleter(CommandController& commandController,
	             SettingsManager& manager);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	SettingsManager& manager;
};

class SettingCompleter : public CommandCompleter
{
public:
	SettingCompleter(CommandController& commandController,
	                 SettingsManager& manager,
	                 const string& name);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	SettingsManager& manager;
};


SettingsManager::SettingsManager(GlobalCommandController& commandController)
	: settingInfo(make_unique<SettingInfo>(
		commandController.getOpenMSXInfoCommand(), *this))
	, setCompleter(make_unique<SetCompleter>(
		commandController, *this))
	, incrCompleter(make_unique<SettingCompleter>(
		commandController, *this, "incr"))
	, unsetCompleter(make_unique<SettingCompleter>(
		commandController, *this, "unset"))
{
}

SettingsManager::~SettingsManager()
{
	assert(settingsMap.empty());
}

void SettingsManager::registerSetting(BaseSetting& setting, string_ref name)
{
	assert(settingsMap.find(name) == end(settingsMap));
	settingsMap[name] = &setting;
}

void SettingsManager::unregisterSetting(BaseSetting& /*setting*/, string_ref name)
{
	assert(settingsMap.find(name) != end(settingsMap));
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
	for (auto& p : settingsMap) {
		auto& setting = *p.second;
		if (setting.needLoadSave()) {
			setting.setString(setting.getRestoreValue());
		}
	}

	// load new values
	auto* settings = config.findChild("settings");
	if (!settings) return;
	for (auto& p : settingsMap) {
		auto name = p.first();
		auto& setting = *p.second;
		if (!setting.needLoadSave()) continue;
		if (auto* elem = settings->findChildWithAttribute(
		                                     "setting", "id", name)) {
			try {
				setting.setString(elem->getData());
			} catch (MSXException&) {
				// ignore, keep default value
			}
		}
	}
}


// class SettingInfo

SettingInfo::SettingInfo(InfoCommand& openMSXInfoCommand,
                         SettingsManager& manager_)
	: InfoTopic(openMSXInfoCommand, "setting")
	, manager(manager_)
{
}

void SettingInfo::execute(
	const vector<TclObject>& tokens, TclObject& result) const
{
	auto& settingsMap = manager.settingsMap;
	switch (tokens.size()) {
	case 2:
		for (auto& p : settingsMap) {
			result.addListElement(p.first());
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

string SettingInfo::help(const vector<string>& /*tokens*/) const
{
	return "openmsx_info setting        : "
	             "returns list of all settings\n"
	       "openmsx_info setting <name> : "
	             "returns info on a specific setting\n";
}

void SettingInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		// complete setting name
		completeString(tokens, keys(manager.settingsMap));
	}
}


// class SetCompleter

SetCompleter::SetCompleter(CommandController& commandController,
                           SettingsManager& manager_)
	: CommandCompleter(commandController, "set"), manager(manager_)
{
}

string SetCompleter::help(const vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		return manager.getByName("set", tokens[1]).getDescription();
	}
	return "Set or query the value of a openMSX setting or Tcl variable\n"
	       "  set <setting>          shows current value\n"
	       "  set <setting> <value>  set a new value\n"
	       "Use 'help set <setting>' to get more info on a specific\n"
	       "openMSX setting.\n";
}

void SetCompleter::tabCompletion(vector<string>& tokens) const
{
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

SettingCompleter::SettingCompleter(
		CommandController& commandController, SettingsManager& manager_,
		const string& name)
	: CommandCompleter(commandController, name)
	, manager(manager_)
{
}

string SettingCompleter::help(const vector<string>& /*tokens*/) const
{
	return ""; // TODO
}

void SettingCompleter::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		// complete setting name
		completeString(tokens, keys(manager.settingsMap));
	}
}

} // namespace openmsx

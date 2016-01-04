#include "SettingsManager.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "XMLElement.hh"
#include "outer.hh"
#include "vla.hh"
#include <cassert>
#include <cstring>

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
	assert(settings.empty());
}

void SettingsManager::registerSetting(BaseSetting& setting)
{
	assert(!settings.contains(setting.getFullNameObj()));
	settings.emplace_noDuplicateCheck(&setting);
}

void SettingsManager::unregisterSetting(BaseSetting& setting)
{
	const auto& name = setting.getFullNameObj();
	assert(settings.contains(name));
	settings.erase(name);
}

BaseSetting* SettingsManager::findSetting(string_ref name) const
{
	auto it = settings.find(name);
	return (it != end(settings)) ? *it : nullptr;
}

BaseSetting* SettingsManager::findSetting(string_ref prefix, string_ref baseName) const
{
	auto size = prefix.size() + baseName.size();
	VLA(char, fullname, size);
	memcpy(&fullname[0],             prefix  .data(), prefix  .size());
	memcpy(&fullname[prefix.size()], baseName.data(), baseName.size());
	return findSetting(string_ref(fullname, size));
}

// Helper functions for setting commands

BaseSetting& SettingsManager::getByName(string_ref cmd, string_ref name) const
{
	if (auto* setting = findSetting(name)) {
		return *setting;
	}
	throw CommandException(cmd + ": " + name + ": no such setting");
}

vector<string> SettingsManager::getTabSettingNames() const
{
	vector<string> result;
	result.reserve(settings.size() * 2);
	for (auto* s : settings) {
		string_ref name = s->getFullName();
		result.push_back(name.str());
		if (name.starts_with("::")) {
			result.push_back(name.substr(2).str());
		} else {
			result.push_back("::" + name);
		}
	}
	return result;
}

void SettingsManager::loadSettings(const XMLElement& config)
{
	// restore default values
	for (auto* s : settings) {
		if (s->needLoadSave()) {
			s->setValue(s->getRestoreValue());
		}
	}

	// load new values
	auto* settingsElem = config.findChild("settings");
	if (!settingsElem) return;
	for (auto* s : settings) {
		if (!s->needLoadSave()) continue;
		if (auto* elem = settingsElem->findChildWithAttribute(
		                "setting", "id", s->getFullName())) {
			try {
				s->setValue(TclObject(elem->getData()));
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
	switch (tokens.size()) {
	case 2:
		for (auto* p : manager.settings) {
			result.addListElement(p->getFullNameObj());
		}
		break;
	case 3: {
		const auto& settingName = tokens[2].getString();
		auto it = manager.settings.find(settingName);
		if (it == end(manager.settings)) {
			throw CommandException("No such setting: " + settingName);
		}
		(*it)->info(result);
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
		completeString(tokens, manager.getTabSettingNames());
	}
}


// class SetCompleter

SettingsManager::SetCompleter::SetCompleter(
		CommandController& commandController_)
	: CommandCompleter(commandController_, "set")
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
	case 2: {
		// complete setting name
		completeString(tokens, manager.getTabSettingNames(), false); // case insensitive
		break;
	}
	case 3: {
		// complete setting value
		auto it = manager.settings.find(tokens[1]);
		if (it != end(manager.settings)) {
			(*it)->tabCompletion(tokens);
		}
		break;
	}
	}
}


// class SettingCompleter

SettingsManager::SettingCompleter::SettingCompleter(
		CommandController& commandController_, SettingsManager& manager_,
		const string& name_)
	: CommandCompleter(commandController_, name_)
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
		completeString(tokens, manager.getTabSettingNames());
	}
}

} // namespace openmsx

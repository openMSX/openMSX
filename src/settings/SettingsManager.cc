#include "SettingsManager.hh"

#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "MSXException.hh"
#include "SettingsConfig.hh"
#include "TclObject.hh"

#include "outer.hh"
#include "view.hh"

#include <cassert>

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

BaseSetting* SettingsManager::findSetting(std::string_view name) const
{
	if (auto it = settings.find(name); it != end(settings)) {
		return *it;
	}
	if (name.starts_with("::")) {
		// try without leading ::
		if (auto it = settings.find(name.substr(2)); it != end(settings)) {
			return *it;
		}
	} else {
		// try adding ::
		if (auto it = settings.find(tmpStrCat("::", name)); it != end(settings)) {
			return *it;
		}
	}
	return nullptr;
}

BaseSetting* SettingsManager::findSetting(std::string_view prefix, std::string_view baseName) const
{
	return findSetting(tmpStrCat(prefix, baseName));
}

// Helper functions for setting commands

BaseSetting& SettingsManager::getByName(std::string_view cmd, std::string_view name) const
{
	if (auto* setting = findSetting(name)) {
		return *setting;
	}
	throw CommandException(cmd, ": ", name, ": no such setting");
}

std::vector<std::string> SettingsManager::getTabSettingNames() const
{
	std::vector<std::string> result;
	result.reserve(size_t(settings.size()) * 2);
	for (const auto* s : settings) {
		std::string_view name = s->getFullName();
		result.emplace_back(name);
		if (name.starts_with("::")) {
			result.emplace_back(name.substr(2));
		} else {
			result.push_back(strCat("::", name));
		}
	}
	return result;
}

void SettingsManager::loadSettings(const SettingsConfig& config) const
{
	// restore default values or load new values
	for (auto* s : settings) {
		if (!s->needLoadSave()) continue;

		if (const auto* savedValue = config.getValueForSetting(s->getBaseName())) {
			try {
				s->setValue(TclObject(*savedValue));
			} catch (MSXException&) {
				// ignore error, instead set default value
				s->setValue(s->getDefaultValue());
			}
		} else {
			s->setValue(s->getDefaultValue());
		}
	}
}


// class SettingInfo

SettingsManager::SettingInfo::SettingInfo(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "setting")
{
}

void SettingsManager::SettingInfo::execute(
	std::span<const TclObject> tokens, TclObject& result) const
{
	auto& manager = OUTER(SettingsManager, settingInfo);
	switch (tokens.size()) {
	case 2:
		result.addListElements(view::transform(
			manager.settings,
			[](auto* p) { return p->getFullNameObj(); }));
		break;
	case 3: {
		const auto& settingName = tokens[2].getString();
		const auto* setting = manager.findSetting(settingName);
		if (!setting) {
			throw CommandException("No such setting: ", settingName);
		}
		try {
			setting->info(result);
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());

		}
		break;
	}
	default:
		throw CommandException("Too many parameters.");
	}
}

std::string SettingsManager::SettingInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "openmsx_info setting        : "
	             "returns list of all settings\n"
	       "openmsx_info setting <name> : "
	             "returns info on a specific setting\n";
}

void SettingsManager::SettingInfo::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		// complete setting name
		const auto& manager = OUTER(SettingsManager, settingInfo);
		completeString(tokens, manager.getTabSettingNames());
	}
}


// class SetCompleter

SettingsManager::SetCompleter::SetCompleter(
		CommandController& commandController_)
	: CommandCompleter(commandController_, "set")
{
}

std::string SettingsManager::SetCompleter::help(std::span<const TclObject> tokens) const
{
	if (tokens.size() == 2) {
		const auto& manager = OUTER(SettingsManager, setCompleter);
		return std::string(manager.getByName("set", tokens[1].getString()).getDescription());
	}
	return "Set or query the value of a openMSX setting or Tcl variable\n"
	       "  set <setting>          shows current value\n"
	       "  set <setting> <value>  set a new value\n"
	       "Use 'help set <setting>' to get more info on a specific\n"
	       "openMSX setting.\n";
}

void SettingsManager::SetCompleter::tabCompletion(std::vector<std::string>& tokens) const
{
	const auto& manager = OUTER(SettingsManager, setCompleter);
	switch (tokens.size()) {
	case 2: {
		// complete setting name
		completeString(tokens, manager.getTabSettingNames(), false); // case insensitive
		break;
	}
	case 3:
		// complete setting value
		if (const auto* setting = manager.findSetting(tokens[1])) {
			setting->tabCompletion(tokens);
		}
		break;
	}
}


// class SettingCompleter

SettingsManager::SettingCompleter::SettingCompleter(
		CommandController& commandController_, SettingsManager& manager_,
		const std::string& name_)
	: CommandCompleter(commandController_, name_)
	, manager(manager_)
{
}

std::string SettingsManager::SettingCompleter::help(std::span<const TclObject> /*tokens*/) const
{
	return {}; // TODO
}

void SettingsManager::SettingCompleter::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 2) {
		// complete setting name
		completeString(tokens, manager.getTabSettingNames());
	}
}

} // namespace openmsx

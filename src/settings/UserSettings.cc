#include "UserSettings.hh"
#include "GlobalCommandController.hh"
#include "SettingsManager.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "checked_cast.hh"
#include "outer.hh"
#include "stl.hh"
#include <cassert>
#include <memory>

using std::string;
using std::vector;
using std::unique_ptr;

namespace openmsx {

// class UserSettings

UserSettings::UserSettings(CommandController& commandController_)
	: userSettingCommand(commandController_)
{
}

void UserSettings::addSetting(unique_ptr<Setting> setting)
{
	assert(!findSetting(setting->getFullName()));
	settings.push_back(std::move(setting));
}

void UserSettings::deleteSetting(Setting& setting)
{
	move_pop_back(settings, rfind_if_unguarded(settings,
		[&](unique_ptr<Setting>& p) { return p.get() == &setting; }));
}

Setting* UserSettings::findSetting(string_view name) const
{
	for (auto& s : settings) {
		if (s->getFullName() == name) {
			return s.get();
		}
	}
	return nullptr;
}


// class UserSettings::Cmd

UserSettings::Cmd::Cmd(CommandController& commandController_)
	: Command(commandController_, "user_setting")
{
}

void UserSettings::Cmd::execute(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw SyntaxError();
	}
	const auto& subCommand = tokens[1].getString();
	if (subCommand == "create") {
		create(tokens, result);
	} else if (subCommand == "destroy") {
		destroy(tokens, result);
	} else if (subCommand == "info") {
		info(tokens, result);
	} else {
		throw CommandException(
			"Invalid subcommand '", subCommand,
			"', expected 'create', 'destroy' or 'info'.");
	}
}

void UserSettings::Cmd::create(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 5) {
		throw SyntaxError();
	}
	const auto& type = tokens[2].getString();
	const auto& settingName = tokens[3].getString();

	auto& controller = checked_cast<GlobalCommandController&>(getCommandController());
	if (controller.getSettingsManager().findSetting(settingName)) {
		throw CommandException(
			"There already exists a setting with this name: ", settingName);
	}

	unique_ptr<Setting> setting;
	if (type == "string") {
		setting = createString(tokens);
	} else if (type == "boolean") {
		setting = createBoolean(tokens);
	} else if (type == "integer") {
		setting = createInteger(tokens);
	} else if (type == "float") {
		setting = createFloat(tokens);
	} else {
		throw CommandException(
			"Invalid setting type '", type, "', expected "
			"'string', 'boolean', 'integer' or 'float'.");
	}
	auto& userSettings = OUTER(UserSettings, userSettingCommand);
	userSettings.addSetting(std::move(setting));

	result.setString(tokens[3].getString()); // name
}

unique_ptr<Setting> UserSettings::Cmd::createString(array_ref<TclObject> tokens)
{
	if (tokens.size() != 6) {
		throw SyntaxError();
	}
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getString();
	return std::make_unique<StringSetting>(
		getCommandController(), sName, desc, initVal);
}

unique_ptr<Setting> UserSettings::Cmd::createBoolean(array_ref<TclObject> tokens)
{
	if (tokens.size() != 6) {
		throw SyntaxError();
	}
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getBoolean(getInterpreter());
	return std::make_unique<BooleanSetting>(
		getCommandController(), sName, desc, initVal);
}

unique_ptr<Setting> UserSettings::Cmd::createInteger(array_ref<TclObject> tokens)
{
	if (tokens.size() != 8) {
		throw SyntaxError();
	}
	auto& interp = getInterpreter();
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getInt(interp);
	const auto& minVal  = tokens[6].getInt(interp);
	const auto& maxVal  = tokens[7].getInt(interp);
	return std::make_unique<IntegerSetting>(
		getCommandController(), sName, desc, initVal, minVal, maxVal);
}

unique_ptr<Setting> UserSettings::Cmd::createFloat(array_ref<TclObject> tokens)
{
	if (tokens.size() != 8) {
		throw SyntaxError();
	}
	auto& interp = getInterpreter();
	const auto& sName    = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getDouble(interp);
	const auto& minVal  = tokens[6].getDouble(interp);
	const auto& maxVal  = tokens[7].getDouble(interp);
	return std::make_unique<FloatSetting>(
		getCommandController(), sName, desc, initVal, minVal, maxVal);
}

void UserSettings::Cmd::destroy(array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	const auto& settingName = tokens[2].getString();

	auto& userSettings = OUTER(UserSettings, userSettingCommand);
	auto* setting = userSettings.findSetting(settingName);
	if (!setting) {
		throw CommandException(
			"There is no user setting with this name: ", settingName);
	}
	userSettings.deleteSetting(*setting);
}

void UserSettings::Cmd::info(array_ref<TclObject> /*tokens*/, TclObject& result)
{
	result.addListElements(getSettingNames());
}

string UserSettings::Cmd::help(const vector<string>& tokens) const
{
	if (tokens.size() < 2) {
		return
		  "Manage user-defined settings.\n"
		  "\n"
		  "User defined settings are mainly used in Tcl scripts "
		  "to create variables (=settings) that are persistent over "
		  "different openMSX sessions.\n"
		  "\n"
		  "  user_setting create <type> <name> <description> <init-value> [<min-value> <max-value>]\n"
		  "  user_setting destroy <name>\n"
		  "  user_setting info\n"
		  "\n"
		  "Use 'help user_setting <subcommand>' to see more info "
		  "on a specific subcommand.";
	}
	assert(tokens.size() >= 2);
	if (tokens[1] == "create") {
		return
		  "user_setting create <type> <name> <description> <init-value> [<min-value> <max-value>]\n"
		  "\n"
		  "Create a user defined setting. The extra arguments have the following meaning:\n"
		  "  <type>         The type for the setting, must be 'string', 'boolean', 'integer' or 'float'.\n"
		  "  <name>         The name for the setting.\n"
		  "  <description>  A (short) description for this setting.\n"
		  "                 This text can be queried via 'help set <setting>'.\n"
		  "  <init-value>   The initial value for the setting.\n"
		  "                 This value is only used the very first time the setting is created, otherwise the value is taken from previous openMSX sessions.\n"
		  "  <min-value>    This parameter is only required for 'integer' and 'float' setting types.\n"
		  "                 Together with max-value this parameter defines the range of valid values.\n"
		  "  <max-value>    See min-value.";

	} else if (tokens[1] == "destroy") {
		return
		  "user_setting destroy <name>\n"
		  "\n"
		  "Remove a previously defined user setting. This only "
		  "removes the setting from the current openMSX session, "
		  "the value of this setting is still preserved for "
		  "future sessions.";

	} else if (tokens[1] == "info") {
		return
		  "user_setting info\n"
		  "\n"
		  "Returns a list of all user defined settings that are "
		  "active in this openMSX session.";

	} else {
		return "No such subcommand, see 'help user_setting'.";
	}
}

void UserSettings::Cmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		static const char* const cmds[] = {
			"create", "destroy", "info"
		};
		completeString(tokens, cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		static const char* const types[] = {
			"string", "boolean", "integer", "float"
		};
		completeString(tokens, types);
	} else if ((tokens.size() == 3) && (tokens[1] == "destroy")) {
		completeString(tokens, getSettingNames());
	}
}

vector<string_view> UserSettings::Cmd::getSettingNames() const
{
	vector<string_view> result;
	auto& userSettings = OUTER(UserSettings, userSettingCommand);
	for (auto& s : userSettings.getSettings()) {
		result.push_back(s->getFullName());
	}
	return result;
}

} // namespace openmsx

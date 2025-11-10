#include "UserSettings.hh"

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "FloatSetting.hh"
#include "IntegerSetting.hh"
#include "SettingsManager.hh"
#include "StringSetting.hh"

#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"

#include "checked_cast.hh"

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>

namespace openmsx {

// class UserSettings

UserSettings::UserSettings(CommandController& commandController_)
	: userSettingCommand(commandController_)
{
}

void UserSettings::addSetting(Info&& info)
{
	assert(!findSetting(info.setting->getFullName()));
	settings.push_back(std::move(info));
}

void UserSettings::deleteSetting(Setting& setting)
{
	move_pop_back(settings, rfind_unguarded(settings, &setting,
		[](auto& info) { return info.setting.get(); }));
}

Setting* UserSettings::findSetting(std::string_view name) const
{
	auto it = std::ranges::find(settings, name, [](auto& info) {
		return info.setting->getFullName(); });
	return (it != end(settings)) ? it->setting.get() : nullptr;
}


// class UserSettings::Cmd

UserSettings::Cmd::Cmd(CommandController& commandController_)
	: Command(commandController_, "user_setting")
{
}

void UserSettings::Cmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{2}, "subcommand ?arg ...?");
	executeSubCommand(tokens[1].getString(),
		"create",  [&]{ create(tokens, result); },
		"destroy", [&]{ destroy(tokens, result); },
		"info",    [&]{ info(tokens, result); });
}

void UserSettings::Cmd::create(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{5}, Prefix{2}, "type name ?arg ...?");
	const auto& type = tokens[2].getString();
	const auto& settingName = tokens[3].getString();

	auto& controller = checked_cast<GlobalCommandController&>(getCommandController());
	if (controller.getSettingsManager().findSetting(settingName)) {
		throw CommandException(
			"There already exists a setting with this name: ", settingName);
	}

	auto getInfo = [&] {
		if (type == "string") {
			return createString(tokens);
		} else if (type == "boolean") {
			return createBoolean(tokens);
		} else if (type == "integer") {
			return createInteger(tokens);
		} else if (type == "float") {
			return createFloat(tokens);
		} else if (type == "enum") {
			return createEnum(tokens);
		} else {
			throw CommandException(
				"Invalid setting type '", type, "', expected "
				"'string', 'boolean', 'integer', 'float' or 'enum'.");
		}
	};
	auto& userSettings = OUTER(UserSettings, userSettingCommand);
	userSettings.addSetting(getInfo());

	result = tokens[3]; // name
}

UserSettings::Info UserSettings::Cmd::createString(std::span<const TclObject> tokens) const
{
	checkNumArgs(tokens, 6, Prefix{3}, "name description initial-value");
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getString();

	auto [storage, view] = make_string_storage(desc);
	return {.setting = std::make_unique<StringSetting>(getCommandController(), sName, view, initVal),
	        .description = std::move(storage)};
}

UserSettings::Info UserSettings::Cmd::createBoolean(std::span<const TclObject> tokens) const
{
	checkNumArgs(tokens, 6, Prefix{3}, "name description initial-value");
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getBoolean(getInterpreter());

	auto [storage, view] = make_string_storage(desc);
	return {.setting = std::make_unique<BooleanSetting>(getCommandController(), sName, view, initVal),
	        .description = std::move(storage)};
}

UserSettings::Info UserSettings::Cmd::createInteger(std::span<const TclObject> tokens) const
{
	checkNumArgs(tokens, 8, Prefix{3}, "name description initial-value min-value max-value");
	auto& interp = getInterpreter();
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getInt(interp);
	const auto& minVal  = tokens[6].getInt(interp);
	const auto& maxVal  = tokens[7].getInt(interp);

	auto [storage, view] = make_string_storage(desc);
	return {.setting = std::make_unique<IntegerSetting>(getCommandController(), sName, view, initVal, minVal, maxVal),
	        .description = std::move(storage)};
}

UserSettings::Info UserSettings::Cmd::createFloat(std::span<const TclObject> tokens) const
{
	checkNumArgs(tokens, 8, Prefix{3}, "name description initial-value min-value max-value");
	auto& interp = getInterpreter();
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initVal = tokens[5].getDouble(interp);
	const auto& minVal  = tokens[6].getDouble(interp);
	const auto& maxVal  = tokens[7].getDouble(interp);

	auto [storage, view] = make_string_storage(desc);
	return {.setting = std::make_unique<FloatSetting>(getCommandController(), sName, view, initVal, minVal, maxVal),
	        .description = std::move(storage)};
}

UserSettings::Info UserSettings::Cmd::createEnum(std::span<const TclObject> tokens) const
{
	checkNumArgs(tokens, 7, Prefix{3}, "name description initial-value allowed-values-list");
	const auto& sName   = tokens[3].getString();
	const auto& desc    = tokens[4].getString();
	const auto& initStr = tokens[5].getString();
	const auto& list    = tokens[6];

	int initVal = -1;
	int i = 0;
	auto map = to_vector(std::views::transform(list, [&](const auto& s) {
		if (s == initStr) initVal = i;
		return EnumSettingBase::MapEntry{std::string(s), i++};
	}));
	if (initVal == -1) {
		throw CommandException(
			"Initial value '", initStr, "' "
			"must be one of the allowed values '",
			list.getString(), '\'');
	}

	auto [storage, view] = make_string_storage(desc);
	return {.setting = std::make_unique<EnumSetting<int>>( getCommandController(), sName, view, initVal, std::move(map)),
	        .description = std::move(storage)};
}

void UserSettings::Cmd::destroy(std::span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 3, "name");
	const auto& settingName = tokens[2].getString();

	auto& userSettings = OUTER(UserSettings, userSettingCommand);
	auto* setting = userSettings.findSetting(settingName);
	if (!setting) {
		throw CommandException(
			"There is no user setting with this name: ", settingName);
	}
	userSettings.deleteSetting(*setting);
}

void UserSettings::Cmd::info(std::span<const TclObject> /*tokens*/, TclObject& result) const
{
	result.addListElements(getSettingNames());
}

std::string UserSettings::Cmd::help(std::span<const TclObject> tokens) const
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
		  "user_setting create <type> <name> <description> <init-value> [<min-value> <max-value> | <value-list>]\n"
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
		  "  <max-value>    See min-value.\n"
		  "  <value-list>   Enum settings have no min and max but instead have a list of possible values";

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

void UserSettings::Cmd::tabCompletion(std::vector<std::string>& tokens) const
{
	using namespace std::literals;
	if (tokens.size() == 2) {
		static constexpr std::array cmds = {
			"create"sv, "destroy"sv, "info"sv,
		};
		completeString(commandController, tokens, cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		static constexpr std::array types = {
			"string"sv, "boolean"sv, "integer"sv, "float"sv, "enum"sv
		};
		completeString(commandController, tokens, types);
	} else if ((tokens.size() == 3) && (tokens[1] == "destroy")) {
		completeString(commandController, tokens, getSettingNames());
	}
}

} // namespace openmsx

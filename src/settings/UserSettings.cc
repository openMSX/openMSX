// $Id$

#include "UserSettings.hh"
#include "Command.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "unreachable.hh"
#include <cassert>

using std::set;
using std::string;
using std::vector;
using std::auto_ptr;

namespace openmsx {

class UserSettingCommand : public Command
{
public:
	UserSettingCommand(UserSettings& userSettings,
	                   CommandController& commandController);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;

private:
	void create (const vector<TclObject*>& tokens, TclObject& result);
	void destroy(const vector<TclObject*>& tokens, TclObject& result);
	void info   (const vector<TclObject*>& tokens, TclObject& result);

	auto_ptr<Setting> createString (const vector<TclObject*>& tokens);
	auto_ptr<Setting> createBoolean(const vector<TclObject*>& tokens);
	auto_ptr<Setting> createInteger(const vector<TclObject*>& tokens);
	auto_ptr<Setting> createFloat  (const vector<TclObject*>& tokens);

	void getSettingNames(set<string>& result) const;

	UserSettings& userSettings;
};


// class UserSettings

UserSettings::UserSettings(CommandController& commandController_)
	: userSettingCommand(new UserSettingCommand(*this, commandController_))
{
}

UserSettings::~UserSettings()
{
	for (Settings::iterator it = settings.begin();
	     it != settings.end(); ++it) {
		delete *it;
	}
}

void UserSettings::addSetting(std::auto_ptr<Setting> setting)
{
	assert(!findSetting(setting->getName()));
	settings.push_back(setting.release());
}

void UserSettings::deleteSetting(Setting& setting)
{
	for (Settings::iterator it = settings.begin();
	     it != settings.end(); ++it) {
		if (*it == &setting) {
			delete *it;
			settings.erase(it);
			return;
		}
	}
	UNREACHABLE;
}

Setting* UserSettings::findSetting(string_ref name) const
{
	for (Settings::const_iterator it = settings.begin();
	     it != settings.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

const UserSettings::Settings& UserSettings::getSettings() const
{
	return settings;
}


// class UserSettingCommand

UserSettingCommand::UserSettingCommand(UserSettings& userSettings_,
                                       CommandController& commandController)
	: Command(commandController, "user_setting")
	, userSettings(userSettings_)
{
}

void UserSettingCommand::execute(const vector<TclObject*>& tokens,
                                   TclObject& result)
{
	if (tokens.size() < 2) {
		throw SyntaxError();
	}
	string_ref subCommand = tokens[1]->getString();
	if (subCommand == "create") {
		create(tokens, result);
	} else if (subCommand == "destroy") {
		destroy(tokens, result);
	} else if (subCommand == "info") {
		info(tokens, result);
	} else {
		throw CommandException(
			"Invalid subcommand '" + subCommand + "', expected "
			"'create', 'destroy' or 'info'.");
	}
}

void UserSettingCommand::create(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() < 5) {
		throw SyntaxError();
	}
	string_ref type = tokens[2]->getString();
	string_ref name = tokens[3]->getString();

	if (getCommandController().findSetting(name)) {
		throw CommandException(
			"There already exists a setting with this name: " + name);
	}

	auto_ptr<Setting> setting;
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
			"Invalid setting type '" + type + "', expected "
			"'string', 'boolean', 'integer' or 'float'.");
	}
	userSettings.addSetting(setting);

	result.setString(tokens[3]->getString()); // name
}

auto_ptr<Setting> UserSettingCommand::createString(const vector<TclObject*>& tokens)
{
	if (tokens.size() != 6) {
		throw SyntaxError();
	}
	string name = tokens[3]->getString().str();
	string desc = tokens[4]->getString().str();
	string initVal = tokens[5]->getString().str();
	return auto_ptr<Setting>(new StringSetting(getCommandController(),
	                                           name, desc, initVal));
}

auto_ptr<Setting> UserSettingCommand::createBoolean(const vector<TclObject*>& tokens)
{
	if (tokens.size() != 6) {
		throw SyntaxError();
	}
	string name = tokens[3]->getString().str();
	string desc = tokens[4]->getString().str();
	bool initVal = tokens[5]->getBoolean();
	return auto_ptr<Setting>(new BooleanSetting(getCommandController(),
	                                            name, desc, initVal));
}

auto_ptr<Setting> UserSettingCommand::createInteger(const vector<TclObject*>& tokens)
{
	if (tokens.size() != 8) {
		throw SyntaxError();
	}
	string name = tokens[3]->getString().str();
	string desc = tokens[4]->getString().str();
	int initVal = tokens[5]->getInt();
	int minVal  = tokens[6]->getInt();
	int maxVal  = tokens[7]->getInt();
	return auto_ptr<Setting>(new IntegerSetting(getCommandController(),
	                                 name, desc, initVal, minVal, maxVal));
}

auto_ptr<Setting> UserSettingCommand::createFloat(const vector<TclObject*>& tokens)
{
	if (tokens.size() != 8) {
		throw SyntaxError();
	}
	string name = tokens[3]->getString().str();
	string desc = tokens[4]->getString().str();
	double initVal = tokens[5]->getInt();
	double minVal  = tokens[6]->getInt();
	double maxVal  = tokens[7]->getInt();
	return auto_ptr<Setting>(new FloatSetting(getCommandController(),
	                                 name, desc, initVal, minVal, maxVal));
}

void UserSettingCommand::destroy(const vector<TclObject*>& tokens,
                                 TclObject& /*result*/)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	string_ref name = tokens[2]->getString();

	Setting* setting = userSettings.findSetting(name);
	if (!setting) {
		throw CommandException(
			"There is no user setting with this name: " + name);
	}
	userSettings.deleteSetting(*setting);
}

void UserSettingCommand::info(const vector<TclObject*>& /*tokens*/,
                              TclObject& result)
{
	set<string> names;
	getSettingNames(names);
	result.addListElements(names.begin(), names.end());
}

string UserSettingCommand::help(const vector<string>& tokens) const
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

void UserSettingCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> s;
	if (tokens.size() == 2) {
		s.insert("create");
		s.insert("destroy");
		s.insert("info");
	} else if ((tokens.size() == 3) && (tokens[1] == "create")) {
		s.insert("string");
		s.insert("boolean");
		s.insert("integer");
		s.insert("float");
	} else if ((tokens.size() == 3) && (tokens[1] == "destroy")) {
		getSettingNames(s);
	}
	completeString(tokens, s);
}

void UserSettingCommand::getSettingNames(set<string>& result) const
{
	const UserSettings::Settings& settings = userSettings.getSettings();
	for (UserSettings::Settings::const_iterator it = settings.begin();
	     it != settings.end(); ++it) {
		result.insert((*it)->getName());
	}
}

} // namespace openmsx

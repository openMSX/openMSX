// $Id$

#include "GlobalCommandController.hh"
#include "Command.hh"
#include "Setting.hh"
#include "ProxyCommand.hh"
#include "ProxySetting.hh"
#include "InfoTopic.hh"
#include "LocalFileReference.hh"
#include "GlobalCliComm.hh"
#include "CliConnection.hh"
#include "HotKey.hh"
#include "Interpreter.hh"
#include "InfoCommand.hh"
#include "CommandException.hh"
#include "SettingsConfig.hh"
#include "SettingsManager.hh"
#include "RomInfoTopic.hh"
#include "TclObject.hh"
#include "Version.hh"
#include "ScopedAssign.hh"
#include "checked_cast.hh"
#include "openmsx.hh"
#include <cassert>
#include <cstdlib>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

class HelpCmd : public Command
{
public:
	explicit HelpCmd(GlobalCommandController& controller);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	GlobalCommandController& controller;
};

class TabCompletionCmd : public Command
{
public:
	explicit TabCompletionCmd(GlobalCommandController& controller);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	GlobalCommandController& controller;
};

class UpdateCmd : public Command
{
public:
	explicit UpdateCmd(CommandController& commandController);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	CliConnection& getConnection();
};

class VersionInfo : public InfoTopic
{
public:
	explicit VersionInfo(InfoCommand& openMSXInfoCommand);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
};


GlobalCommandController::GlobalCommandController(
	EventDistributor& eventDistributor,
	GlobalCliComm& cliComm_, Reactor& reactor_)
	: cliComm(cliComm_)
	, connection(NULL)
	, reactor(reactor_)
	, interpreter(new Interpreter(eventDistributor))
	, openMSXInfoCommand(new InfoCommand(*this, "openmsx_info"))
	, hotKey(new HotKey(*this, eventDistributor))
	, helpCmd(new HelpCmd(*this))
	, tabCompletionCmd(new TabCompletionCmd(*this))
	, proxyCmd(new ProxyCmd(*this, reactor))
	, versionInfo(new VersionInfo(getOpenMSXInfoCommand()))
	, romInfoTopic(new RomInfoTopic(getOpenMSXInfoCommand()))
{
	// For backwards compatibility:
	//  In the past we had an openMSX command 'update'. This was a mistake
	//  because it overlaps with the native Tcl command with the same name.
	//  We renamed 'update' to 'openmsx_update'. And installed a wrapper
	//  around 'update' that either forwards to the native Tcl command or
	//  to the 'openmsx_update' command.
	//  In future openMSX versions this wrapper will be removed.
	interpreter->execute("rename update __tcl_update");
	interpreter->execute(
		"proc update { args } {\n"
		"    if {$args == \"\"} {\n"
		"        __tcl_update\n"
		"    } elseif {$args == \"idletasks\"} {\n"
		"        __tcl_update idletasks\n"
		"    } else {\n"
		"        puts stderr \"Warning: the openMSX \\'update\\' command "
		                      "overlapped with a native Tcl command "
		                      "and has been renamed to \\'openmsx_update\\'. "
		                      "In future openMSX releases this forwarder "
		                      "will stop working, so please change your "
		                      "scripts to use the \\'openmsx_update\\' "
		                      "command instead of \\'update\\'.\"\n"
		"        eval \"openmsx_update $args\"\n"
		"    }\n"
		"}\n");
	updateCmd.reset(new UpdateCmd(*this));
}

GlobalCommandController::~GlobalCommandController()
{
	// all this reset() stuff is also done automatically by the destructor,
	// but we need it slightly earlier to test the assertions.
	// TODO find a cleaner way to do this
	romInfoTopic.reset();
	versionInfo.reset();
	updateCmd.reset();
	tabCompletionCmd.reset();
	helpCmd.reset();
	settingsConfig.reset();
	hotKey.reset();
	openMSXInfoCommand.reset();

	assert(commands.empty());
	assert(commandCompleters.empty());
}

void GlobalCommandController::registerProxyCommand(const string& name)
{
	if (proxyCommandMap[name] == 0) {
		registerCommand(*proxyCmd, name);
		registerCompleter(*proxyCmd, name);
	}
	++proxyCommandMap[name];
}

void GlobalCommandController::unregisterProxyCommand(string_ref name)
{
	assert(proxyCommandMap[name]);
	--proxyCommandMap[name];
	if (proxyCommandMap[name] == 0) {
		unregisterCompleter(*proxyCmd, name);
		unregisterCommand(*proxyCmd, name);
	}
}

GlobalCommandController::ProxySettings::iterator
GlobalCommandController::findProxySetting(const std::string& name)
{
	for (ProxySettings::iterator it = proxySettings.begin();
	     it != proxySettings.end(); ++it) {
		if (it->first->getName() == name) {
			return it;
		}
	}
	return proxySettings.end();
}

void GlobalCommandController::registerProxySetting(Setting& setting)
{
	const string& name = setting.getName();
	ProxySettings::iterator it = findProxySetting(name);
	if (it == proxySettings.end()) {
		// first occurrence
		ProxySetting* proxy = new ProxySetting(*this, reactor, name);
		proxySettings.push_back(std::make_pair(proxy, 1));
		getSettingsConfig().getSettingsManager().registerSetting(*proxy, name);
		getInterpreter().registerSetting(*proxy, name);
	} else {
		// was already registered
		++(it->second);
	}
}

void GlobalCommandController::unregisterProxySetting(Setting& setting)
{
	const string& name = setting.getName();
	ProxySettings::iterator it = findProxySetting(name);
	assert(it != proxySettings.end());
	assert(it->second);
	--(it->second);
	if (it->second == 0) {
		ProxySetting* proxy = it->first;
		getInterpreter().unregisterSetting(*proxy, name);
		getSettingsConfig().getSettingsManager().unregisterSetting(*proxy, name);
		proxySettings.erase(it);
		delete proxy;
	}
}

CliComm& GlobalCommandController::getCliComm()
{
	return cliComm;
}

CliConnection* GlobalCommandController::getConnection() const
{
	return connection;
}

Interpreter& GlobalCommandController::getInterpreter()
{
	return *interpreter;
}

InfoCommand& GlobalCommandController::getOpenMSXInfoCommand()
{
	return *openMSXInfoCommand;
}

SettingsConfig& GlobalCommandController::getSettingsConfig()
{
	if (!settingsConfig.get()) {
		settingsConfig.reset(new SettingsConfig(*this, *hotKey));
	}
	return *settingsConfig;
}

void GlobalCommandController::registerCommand(
	Command& command, const string& str)
{
	assert(commands.find(str) == commands.end());

	commands[str] = &command;
	interpreter->registerCommand(str, command);
}

void GlobalCommandController::unregisterCommand(
	Command& command, string_ref str)
{
	assert(commands.find(str) != commands.end());
	assert(commands.find(str)->second == &command);

	interpreter->unregisterCommand(str, command);
	commands.erase(str);
}

void GlobalCommandController::registerCompleter(
	CommandCompleter& completer, string_ref str)
{
	assert(commandCompleters.find(str) == commandCompleters.end());
	commandCompleters[str] = &completer;
}

void GlobalCommandController::unregisterCompleter(
	CommandCompleter& completer, string_ref str)
{
	(void)completer;
	assert(commandCompleters.find(str) != commandCompleters.end());
	assert(commandCompleters.find(str)->second == &completer);
	commandCompleters.erase(str);
}

void GlobalCommandController::registerSetting(Setting& setting)
{
	const string& name = setting.getName();
	getSettingsConfig().getSettingsManager().registerSetting(setting, name);
	interpreter->registerSetting(setting, name);
}

void GlobalCommandController::unregisterSetting(Setting& setting)
{
	const string& name = setting.getName();
	interpreter->unregisterSetting(setting, name);
	getSettingsConfig().getSettingsManager().unregisterSetting(setting, name);
}

Setting* GlobalCommandController::findSetting(string_ref name)
{
	return getSettingsConfig().getSettingsManager().findSetting(name);
}

void GlobalCommandController::changeSetting(
	const std::string& name, const string& value)
{
	interpreter->setVariable(name, value);
}

void GlobalCommandController::changeSetting(Setting& setting, const string& value)
{
	changeSetting(setting.getName(), value);
}

bool GlobalCommandController::hasCommand(string_ref command) const
{
	return commands.find(command) != commands.end();
}

void GlobalCommandController::split(string_ref str, vector<string>& tokens,
                                    const char delimiter)
{
	enum ParseState {Alpha, BackSlash, Quote};
	ParseState state = Alpha;

	for (unsigned i = 0; i < str.size(); ++i) {
		char chr = str[i];
		switch (state) {
			case Alpha:
				if (tokens.empty()) {
					tokens.push_back("");
				}
				if (chr == delimiter) {
					// token done, start new token
					tokens.push_back("");
				} else {
					tokens.back() += chr;
					if (chr == '\\') {
						state = BackSlash;
					} else if (chr == '"') {
						state = Quote;
					}
				}
				break;
			case Quote:
				tokens.back() += chr;
				if (chr == '"') {
					state = Alpha;
				}
				break;
			case BackSlash:
				tokens.back() += chr;
				state = Alpha;
				break;
		}
	}
}

string GlobalCommandController::removeEscaping(const string& str)
{
	enum ParseState {Alpha, BackSlash, Quote};
	ParseState state = Alpha;

	string result;
	for (unsigned i = 0; i < str.length(); ++i) {
		char chr = str[i];
		switch (state) {
			case Alpha:
				if (chr == '\\') {
					state = BackSlash;
				} else if (chr == '"') {
					state = Quote;
				} else {
					result += chr;
				}
				break;
			case Quote:
				if (chr == '"') {
					state = Alpha;
				} else {
					result += chr;
				}
				break;
			case BackSlash:
				result += chr;
				state = Alpha;
				break;
		}
	}
	return result;
}

void GlobalCommandController::removeEscaping(
	const vector<string>& input, vector<string>& result,
	bool keepLastIfEmpty)
{
	for (vector<string>::const_iterator it = input.begin();
	     it != input.end();
	     ++it) {
		if (!it->empty()) {
			result.push_back(removeEscaping(*it));
		}
	}
	if (keepLastIfEmpty && (input.empty() || input.back().empty())) {
		result.push_back("");
	}
}

static string escapeChars(const string& str, const string& chars)
{
	string result;
	for (unsigned i = 0; i < str.length(); ++i) {
		char chr = str[i];
		if (chars.find(chr) != string::npos) {
			result += '\\';
		}
		result += chr;

	}
	return result;
}

string GlobalCommandController::addEscaping(const string& str, bool quote,
                                            bool finished)
{
	if (str.empty() && finished) {
		quote = true;
	}
	string result = escapeChars(str, "$[]");
	if (quote) {
		result = '"' + result;
		if (finished) {
			result += '"';
		}
	} else {
		result = escapeChars(result, " ");
	}
	return result;
}

string GlobalCommandController::join(
	const vector<string>& tokens, char delimiter)
{
	string result;
	bool first = true;
	for (vector<string>::const_iterator it = tokens.begin();
	     it != tokens.end();
	     ++it) {
		if (!first) {
			result += delimiter;
		}
		first = false;
		result += *it;
	}
	return result;
}

bool GlobalCommandController::isComplete(const string& command)
{
	return interpreter->isComplete(command);
}

string GlobalCommandController::executeCommand(
	const string& cmd, CliConnection* connection_)
{
	ScopedAssign<CliConnection*> sa(connection, connection_);
	return interpreter->execute(cmd);
}

void GlobalCommandController::splitList(
	const string& list, vector<string>& result)
{
	interpreter->splitList(list, result);
}

void GlobalCommandController::source(const string& script)
{
	try {
		LocalFileReference file(script);
		interpreter->executeFile(file.getFilename());
	} catch (CommandException& e) {
		getCliComm().printWarning(
			 "While executing " + script + ": " + e.getMessage());
	}
}

string GlobalCommandController::tabCompletion(string_ref command)
{
	// split on 'active' command (the command that should actually be
	// completed). Some examples:
	//    if {[debug rea<tab> <-- should complete the 'debug' command
	//                              instead of the 'if' command
	//    bind F6 { cycl<tab> <-- should complete 'cycle' instead of 'bind'
	TclParser parser = interpreter->parse(command);
	int last = parser.getLast();
	string_ref pre  = command.substr(0, last);
	string_ref post = command.substr(last);

	// split command string in tokens
	vector<string> originalTokens;
	split(post, originalTokens, ' ');
	if (originalTokens.empty()) {
		originalTokens.push_back("");
	}

	// complete last token
	vector<string> tokens;
	removeEscaping(originalTokens, tokens, true);
	size_t oldNum = tokens.size();
	tabCompletion(tokens);
	size_t newNum = tokens.size();
	bool tokenFinished = oldNum != newNum;

	// replace last token
	string& original = originalTokens.back();
	string& completed = tokens[oldNum - 1];
	if (!completed.empty()) {
		bool quote = !original.empty() && (original[0] == '"');
		original = addEscaping(completed, quote, tokenFinished);
	}
	if (tokenFinished) {
		assert(newNum == (oldNum + 1));
		assert(tokens.back().empty());
		originalTokens.push_back("");
	}

	// rebuild command string
	return pre + join(originalTokens, ' ');
}

void GlobalCommandController::tabCompletion(vector<string>& tokens)
{
	if (tokens.empty()) {
		// nothing typed yet
		return;
	}
	if (tokens.size() == 1) {
		// build a list of all command strings
		set<string> cmds;
		interpreter->getCommandNames(cmds);
		Completer::completeString(tokens, cmds);
	} else {
		CompleterMap::const_iterator it = commandCompleters.find(tokens.front());
		if (it != commandCompleters.end()) {
			it->second->tabCompletion(tokens);
		} else {
			TclObject command(*interpreter);
			command.addListElement("openmsx::tabcompletion");
			command.addListElements(tokens.begin(), tokens.end());
			try {
				string result = command.executeCommand();
				vector<string> split;
				splitList(result, split);
				bool sensitive = true;
				if (!split.empty()) {
					if (split.back() == "false") {
						split.pop_back();
						sensitive = false;
					} else if (split.back() == "true") {
						split.pop_back();
						sensitive = true;
					}
				}
				set<string> completions(split.begin(), split.end());
				Completer::completeString(tokens, completions, sensitive);
			} catch (CommandException& e) {
				cliComm.printWarning(
					"Error while executing tab-completion "
					"proc: " + e.getMessage());
			}
		}
	}
}


// Help Command

HelpCmd::HelpCmd(GlobalCommandController& controller_)
	: Command(controller_, "help")
	, controller(controller_)
{
}

void HelpCmd::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	switch (tokens.size()) {
	case 1: {
		string text =
			"Use 'help [command]' to get help for a specific command\n"
			"The following commands exist:\n";
		for (GlobalCommandController::CompleterMap::const_iterator it =
		         controller.commandCompleters.begin();
		     it != controller.commandCompleters.end(); ++it) {
			string_ref key = it->first();
			text.append(key.data(), key.size());
			text += '\n';
		}
		result.setString(text);
		break;
	}
	default: {
		 GlobalCommandController::CompleterMap::const_iterator it =
			controller.commandCompleters.find(tokens[1]->getString());
		if (it != controller.commandCompleters.end()) {
			vector<string> tokens2;
			vector<TclObject*>::const_iterator it2 = tokens.begin();
			for (++it2; it2 != tokens.end(); ++it2) {
				tokens2.push_back((*it2)->getString().str());
			}
			result.setString(it->second->help(tokens2));
		} else {
			TclObject command(result.getInterpreter());
			command.addListElement("openmsx::help");
			vector<TclObject*>::const_iterator it2 = tokens.begin();
			for (++it2; it2 != tokens.end(); ++it2) {
				command.addListElement(**it2);
			}
			result.setString(command.executeCommand());
		}
		break;
	}
	}
}

string HelpCmd::help(const vector<string>& /*tokens*/) const
{
	return "prints help information for commands\n";
}

void HelpCmd::tabCompletion(vector<string>& tokens) const
{
	string front = tokens.front();
	tokens.erase(tokens.begin());
	controller.tabCompletion(tokens);
	tokens.insert(tokens.begin(), front);
}


// TabCompletionCmd Command

TabCompletionCmd::TabCompletionCmd(GlobalCommandController& controller_)
	: Command(controller_, "tabcompletion")
	, controller(controller_)
{
}

void TabCompletionCmd::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	switch (tokens.size()) {
	case 2: {
		// TODO this prints list of possible completions in the console
		string_ref command = tokens[1]->getString();
		result.setString(controller.tabCompletion(command));
		break;
	}
	default:
		throw SyntaxError();
	}
}

string TabCompletionCmd::help(const vector<string>& /*tokens*/) const
{
	return "!!! This command will change in the future !!!\n"
	       "Tries to completes the given argument as if it were typed in "
	       "the console. This command is only useful to provide "
	       "tabcompletion to external console interfaces.";
}


// class UpdateCmd

UpdateCmd::UpdateCmd(CommandController& commandController)
	: Command(commandController, "openmsx_update")
{
}

static GlobalCliComm::UpdateType getType(const string& name)
{
	const char* const* updateStr = CliComm::getUpdateStrings();
	for (unsigned i = 0; i < CliComm::NUM_UPDATES; ++i) {
		if (updateStr[i] == name) {
			return static_cast<CliComm::UpdateType>(i);
		}
	}
	throw CommandException("No such update type: " + name);
}

CliConnection& UpdateCmd::getConnection()
{
	GlobalCommandController* controller =
		checked_cast<GlobalCommandController*>(&getCommandController());
	CliConnection* connection = controller->getConnection();
	if (!connection) {
		throw CommandException("This command only makes sense when "
		                       "it's used from an external application.");
	}
	return *connection;
}

string UpdateCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	if (tokens[1] == "enable") {
		getConnection().setUpdateEnable(getType(tokens[2]), true);
	} else if (tokens[1] == "disable") {
		getConnection().setUpdateEnable(getType(tokens[2]), false);
	} else {
		throw SyntaxError();
	}
	return "";
}

string UpdateCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "Enable or disable update events for external applications. See doc/openmsx-control-xml.txt.";
	return helpText;
}

void UpdateCmd::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> ops;
		ops.insert("enable");
		ops.insert("disable");
		completeString(tokens, ops);
		break;
	}
	case 3: {
		const char* const* updateStr = CliComm::getUpdateStrings();
		set<string> types(updateStr, updateStr + CliComm::NUM_UPDATES);
		completeString(tokens, types);
	}
	}
}


// Version info

VersionInfo::VersionInfo(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "version")
{
}

void VersionInfo::execute(const vector<TclObject*>& /*tokens*/,
                          TclObject& result) const
{
	result.setString(Version::FULL_VERSION);
}

string VersionInfo::help(const vector<string>& /*tokens*/) const
{
	return "Prints openMSX version.";
}

} // namespace openmsx

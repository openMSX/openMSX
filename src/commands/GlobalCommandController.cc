#include "GlobalCommandController.hh"
#include "Reactor.hh"
#include "Setting.hh"
#include "ProxyCommand.hh"
#include "ProxySetting.hh"
#include "LocalFileReference.hh"
#include "GlobalCliComm.hh"
#include "CliConnection.hh"
#include "CommandException.hh"
#include "SettingsManager.hh"
#include "TclObject.hh"
#include "Version.hh"
#include "KeyRange.hh"
#include "ScopedAssign.hh"
#include "StringOp.hh"
#include "checked_cast.hh"
#include "memory.hh"
#include "outer.hh"
#include "xrange.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

GlobalCommandController::GlobalCommandController(
	EventDistributor& eventDistributor,
	GlobalCliComm& cliComm_, Reactor& reactor_)
	: cliComm(cliComm_)
	, connection(nullptr)
	, reactor(reactor_)
	, interpreter(eventDistributor)
	, openMSXInfoCommand(*this, "openmsx_info")
	, hotKey(reactor.getRTScheduler(), *this, eventDistributor)
	, settingsConfig(*this, hotKey)
	, helpCmd(*this)
	, tabCompletionCmd(*this)
	, updateCmd(*this)
	, platformInfo(getOpenMSXInfoCommand())
	, versionInfo (getOpenMSXInfoCommand())
	, romInfoTopic(getOpenMSXInfoCommand())
{
}

GlobalCommandController::~GlobalCommandController() = default;

GlobalCommandControllerBase::~GlobalCommandControllerBase()
{
	// GlobalCommandController destructor must have run before
	// we can check this.
	assert(commands.empty());
	assert(commandCompleters.empty());
}

void GlobalCommandController::registerProxyCommand(const string& name)
{
	auto it = proxyCommandMap.find(name);
	if (it == end(proxyCommandMap)) {
		it = proxyCommandMap.emplace_noDuplicateCheck(
			0, make_unique<ProxyCmd>(reactor, name));
	}
	++it->first;
}

void GlobalCommandController::unregisterProxyCommand(string_ref name)
{
	auto it = proxyCommandMap.find(name);
	assert(it != end(proxyCommandMap));
	assert(it->first > 0);
	--it->first;
	if (it->first == 0) {
		proxyCommandMap.erase(it);
	}
}

GlobalCommandController::ProxySettings::iterator
GlobalCommandController::findProxySetting(string_ref name)
{
	return find_if(begin(proxySettings), end(proxySettings),
		[&](ProxySettings::value_type& v) { return v.first->getFullName() == name; });
}

void GlobalCommandController::registerProxySetting(Setting& setting)
{
	const auto& name = setting.getBaseNameObj();
	auto it = findProxySetting(name.getString());
	if (it == end(proxySettings)) {
		// first occurrence
		auto proxy = make_unique<ProxySetting>(reactor, name);
		getSettingsManager().registerSetting(*proxy);
		getInterpreter().registerSetting(*proxy);
		proxySettings.emplace_back(std::move(proxy), 1);
	} else {
		// was already registered
		++(it->second);
	}
}

void GlobalCommandController::unregisterProxySetting(Setting& setting)
{
	auto it = findProxySetting(setting.getBaseName());
	assert(it != end(proxySettings));
	assert(it->second);
	--(it->second);
	if (it->second == 0) {
		auto& proxy = *it->first;
		getInterpreter().unregisterSetting(proxy);
		getSettingsManager().unregisterSetting(proxy);
		move_pop_back(proxySettings, it);
	}
}

CliComm& GlobalCommandController::getCliComm()
{
	return cliComm;
}

Interpreter& GlobalCommandController::getInterpreter()
{
	return interpreter;
}

void GlobalCommandController::registerCommand(
	Command& command, const string& str)
{
	assert(!commands.contains(str));
	commands.emplace_noDuplicateCheck(str, &command);
	interpreter.registerCommand(str, command);
}

void GlobalCommandController::unregisterCommand(
	Command& command, string_ref str)
{
	assert(commands.contains(str));
	assert(commands[str.str()] == &command);
	commands.erase(str);
	interpreter.unregisterCommand(command);
}

void GlobalCommandController::registerCompleter(
	CommandCompleter& completer, string_ref str)
{
	if (str.starts_with("::")) str = str.substr(2); // drop leading ::
	assert(!commandCompleters.contains(str));
	commandCompleters.emplace_noDuplicateCheck(str.str(), &completer);
}

void GlobalCommandController::unregisterCompleter(
	CommandCompleter& completer, string_ref str)
{
	if (str.starts_with("::")) str = str.substr(2); // drop leading ::
	assert(commandCompleters.contains(str));
	assert(commandCompleters[str.str()] == &completer); (void)completer;
	commandCompleters.erase(str);
}

void GlobalCommandController::registerSetting(Setting& setting)
{
	getSettingsManager().registerSetting(setting);
	interpreter.registerSetting(setting);
}

void GlobalCommandController::unregisterSetting(Setting& setting)
{
	interpreter.unregisterSetting(setting);
	getSettingsManager().unregisterSetting(setting);
}

bool GlobalCommandController::hasCommand(string_ref command) const
{
	return commands.find(command) != end(commands);
}

void GlobalCommandController::split(string_ref str, vector<string>& tokens,
                                    const char delimiter)
{
	enum ParseState {Alpha, BackSlash, Quote};
	ParseState state = Alpha;

	for (auto chr : str) {
		switch (state) {
			case Alpha:
				if (tokens.empty()) {
					tokens.emplace_back();
				}
				if (chr == delimiter) {
					// token done, start new token
					tokens.emplace_back();
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
	for (auto chr : str) {
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

vector<string> GlobalCommandController::removeEscaping(
	const vector<string>& input, bool keepLastIfEmpty)
{
	vector<string> result;
	for (auto& s : input) {
		if (!s.empty()) {
			result.push_back(removeEscaping(s));
		}
	}
	if (keepLastIfEmpty && (input.empty() || input.back().empty())) {
		result.emplace_back();
	}
	return result;
}

static string escapeChars(const string& str, const string& chars)
{
	string result;
	for (auto chr : str) {
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
	StringOp::Builder result;
	bool first = true;
	for (auto& t : tokens) {
		if (!first) {
			result << delimiter;
		}
		first = false;
		result << t;
	}
	return result;
}

bool GlobalCommandController::isComplete(const string& command)
{
	return interpreter.isComplete(command);
}

TclObject GlobalCommandController::executeCommand(
	const string& cmd, CliConnection* connection_)
{
	ScopedAssign<CliConnection*> sa(connection, connection_);
	return interpreter.execute(cmd);
}

void GlobalCommandController::source(const string& script)
{
	try {
		LocalFileReference file(script);
		interpreter.executeFile(file.getFilename());
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
	TclParser parser = interpreter.parse(command);
	int last = parser.getLast();
	string_ref pre  = command.substr(0, last);
	string_ref post = command.substr(last);

	// split command string in tokens
	vector<string> originalTokens;
	split(post, originalTokens, ' ');
	if (originalTokens.empty()) {
		originalTokens.emplace_back();
	}

	// complete last token
	auto tokens = removeEscaping(originalTokens, true);
	auto oldNum = tokens.size();
	tabCompletion(tokens);
	auto newNum = tokens.size();
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
		originalTokens.emplace_back();
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
		string_ref cmd = tokens[0];
		string_ref leadingNs;
		// remove leading ::
		if (cmd.starts_with("::")) {
			cmd.remove_prefix(2);
			leadingNs = "::";
		}
		// get current (typed) namespace
		auto p1 = cmd.rfind("::");
		string_ref ns = (p1 == string_ref::npos) ? cmd : cmd.substr(0, p1 + 2);

		// build a list of all command strings
		TclObject names = interpreter.getCommandNames();
		vector<string> names2;
		names2.reserve(names.size());
		for (string_ref n1 : names) {
			// remove leading ::
			if (n1.starts_with("::")) n1.remove_prefix(2);
			// initial namespace part must match
			if (!n1.starts_with(ns)) continue;
			// the part following the initial namespace
			string_ref n2 = n1.substr(ns.size());
			// only keep upto the next namespace portion,
			auto p2 = n2.find("::");
			auto n3 = (p2 == string_ref::npos) ? n1 : n1.substr(0, ns.size() + p2 + 2);
			// don't care about adding the same string multiple times
			names2.push_back(leadingNs + n3);
		}
		Completer::completeString(tokens, names2);
	} else {
		string_ref cmd = tokens.front();
		if (cmd.starts_with("::")) cmd = cmd.substr(2); // drop leading ::

		auto it = commandCompleters.find(cmd);
		if (it != end(commandCompleters)) {
			it->second->tabCompletion(tokens);
		} else {
			TclObject command;
			command.addListElement("openmsx::tabcompletion");
			command.addListElements(tokens);
			try {
				TclObject list = command.executeCommand(interpreter);
				bool sensitive = true;
				auto begin = list.begin();
				auto end   = list.end();
				if (begin != end) {
					auto it2 = end; --it2;
					auto back = *it2;
					if (back == "false") {
						end = it2;
						sensitive = false;
					} else if (back == "true") {
						end = it2;
						sensitive = true;
					}
				}
				Completer::completeString(tokens, begin, end, sensitive);
			} catch (CommandException& e) {
				cliComm.printWarning(
					"Error while executing tab-completion "
					"proc: " + e.getMessage());
			}
		}
	}
}


// Help Command

GlobalCommandController::HelpCmd::HelpCmd(GlobalCommandController& controller_)
	: Command(controller_, "help")
{
}

void GlobalCommandController::HelpCmd::execute(
	array_ref<TclObject> tokens, TclObject& result)
{
	auto& controller = OUTER(GlobalCommandController, helpCmd);
	switch (tokens.size()) {
	case 1: {
		string text =
			"Use 'help [command]' to get help for a specific command\n"
			"The following commands exist:\n";
		const auto& k = keys(controller.commandCompleters);
		vector<string_ref> cmds(begin(k), end(k));
		std::sort(begin(cmds), end(cmds));
		for (auto& line : formatListInColumns(cmds)) {
			text += line;
			text += '\n';
		}
		result.setString(text);
		break;
	}
	default: {
		auto it = controller.commandCompleters.find(tokens[1].getString());
		if (it != end(controller.commandCompleters)) {
			vector<string> tokens2;
			auto it2 = std::begin(tokens);
			for (++it2; it2 != std::end(tokens); ++it2) {
				tokens2.push_back(it2->getString().str());
			}
			result.setString(it->second->help(tokens2));
		} else {
			TclObject command;
			command.addListElement("openmsx::help");
			command.addListElements(std::begin(tokens) + 1, std::end(tokens));
			result = command.executeCommand(getInterpreter());
		}
		break;
	}
	}
}

string GlobalCommandController::HelpCmd::help(const vector<string>& /*tokens*/) const
{
	return "prints help information for commands\n";
}

void GlobalCommandController::HelpCmd::tabCompletion(vector<string>& tokens) const
{
	string front = std::move(tokens.front());
	tokens.erase(begin(tokens));
	auto& controller = OUTER(GlobalCommandController, helpCmd);
	controller.tabCompletion(tokens);
	tokens.insert(begin(tokens), std::move(front));
}


// TabCompletionCmd Command

GlobalCommandController::TabCompletionCmd::TabCompletionCmd(
		GlobalCommandController& controller_)
	: Command(controller_, "tabcompletion")
{
}

void GlobalCommandController::TabCompletionCmd::execute(
	array_ref<TclObject> tokens, TclObject& result)
{
	switch (tokens.size()) {
	case 2: {
		// TODO this prints list of possible completions in the console
		auto& controller = OUTER(GlobalCommandController, tabCompletionCmd);
		result.setString(controller.tabCompletion(tokens[1].getString()));
		break;
	}
	default:
		throw SyntaxError();
	}
}

string GlobalCommandController::TabCompletionCmd::help(const vector<string>& /*tokens*/) const
{
	return "!!! This command will change in the future !!!\n"
	       "Tries to completes the given argument as if it were typed in "
	       "the console. This command is only useful to provide "
	       "tabcompletion to external console interfaces.";
}


// class UpdateCmd

GlobalCommandController::UpdateCmd::UpdateCmd(CommandController& commandController_)
	: Command(commandController_, "openmsx_update")
{
}

static GlobalCliComm::UpdateType getType(const TclObject& name)
{
	auto updateStr = CliComm::getUpdateStrings();
	for (auto i : xrange(updateStr.size())) {
		if (updateStr[i] == name) {
			return static_cast<CliComm::UpdateType>(i);
		}
	}
	throw CommandException("No such update type: " + name.getString());
}

CliConnection& GlobalCommandController::UpdateCmd::getConnection()
{
	auto& controller = OUTER(GlobalCommandController, updateCmd);
	if (auto* c = controller.getConnection()) {
		return *c;
	}
	throw CommandException("This command only makes sense when "
	                       "it's used from an external application.");
}

void GlobalCommandController::UpdateCmd::execute(
	array_ref<TclObject> tokens, TclObject& /*result*/)
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
}

string GlobalCommandController::UpdateCmd::help(const vector<string>& /*tokens*/) const
{
	static const string helpText = "Enable or disable update events for external applications. See doc/openmsx-control-xml.txt.";
	return helpText;
}

void GlobalCommandController::UpdateCmd::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
	case 2: {
		static const char* const ops[] = { "enable", "disable" };
		completeString(tokens, ops);
		break;
	}
	case 3:
		completeString(tokens, CliComm::getUpdateStrings());
		break;
	}
}


// Platform info

GlobalCommandController::PlatformInfo::PlatformInfo(InfoCommand& openMSXInfoCommand_)
	: InfoTopic(openMSXInfoCommand_, "platform")
{
}

void GlobalCommandController::PlatformInfo::execute(
	array_ref<TclObject> /*tokens*/, TclObject& result) const
{
	result.setString(TARGET_PLATFORM);
}

string GlobalCommandController::PlatformInfo::help(const vector<string>& /*tokens*/) const
{
	return "Prints openMSX platform.";
}

// Version info

GlobalCommandController::VersionInfo::VersionInfo(InfoCommand& openMSXInfoCommand_)
	: InfoTopic(openMSXInfoCommand_, "version")
{
}

void GlobalCommandController::VersionInfo::execute(
	array_ref<TclObject> /*tokens*/, TclObject& result) const
{
	result.setString(Version::full());
}

string GlobalCommandController::VersionInfo::help(const vector<string>& /*tokens*/) const
{
	return "Prints openMSX version.";
}

} // namespace openmsx

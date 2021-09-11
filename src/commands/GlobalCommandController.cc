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
#include "ScopedAssign.hh"
#include "join.hh"
#include "outer.hh"
#include "ranges.hh"
#include "stl.hh"
#include "StringOp.hh"
#include "view.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <cassert>
#include <memory>

using std::string;
using std::string_view;
using std::vector;

namespace openmsx {

GlobalCommandController::GlobalCommandController(
	EventDistributor& eventDistributor,
	GlobalCliComm& cliComm_, Reactor& reactor_)
	: cliComm(cliComm_)
	, connection(nullptr)
	, reactor(reactor_)
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
#ifdef DEBUG
	assert(commands.empty());
#endif
	assert(commandCompleters.empty());
}

void GlobalCommandController::registerProxyCommand(std::string_view name)
{
	auto it = proxyCommandMap.find(name);
	if (it == end(proxyCommandMap)) {
		it = proxyCommandMap.emplace_noDuplicateCheck(
			0, std::make_unique<ProxyCmd>(reactor, name));
	}
	++it->first;
}

void GlobalCommandController::unregisterProxyCommand(string_view name)
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
GlobalCommandController::findProxySetting(string_view name)
{
	return ranges::find(proxySettings, name,
		[](auto& v) { return v.first->getFullName(); });
}

void GlobalCommandController::registerProxySetting(Setting& setting)
{
	const auto& name = setting.getBaseNameObj();
	auto it = findProxySetting(name.getString());
	if (it == end(proxySettings)) {
		// first occurrence
		auto proxy = std::make_unique<ProxySetting>(reactor, name);
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
	Command& command, zstring_view str)
{
#ifdef DEBUG
	assert(!commands.contains(str));
	commands.emplace_noDuplicateCheck(str, &command);
#endif
	interpreter.registerCommand(str, command);
}

void GlobalCommandController::unregisterCommand(
	Command& command, string_view str)
{
	(void)str;
#ifdef DEBUG
	assert(commands.contains(str));
	assert(commands[str] == &command);
	commands.erase(str);
#endif
	interpreter.unregisterCommand(command);
}

void GlobalCommandController::registerCompleter(
	CommandCompleter& completer, string_view str)
{
	if (StringOp::startsWith(str, "::")) str.remove_prefix(2); // drop leading ::
	assert(!commandCompleters.contains(str));
	commandCompleters.emplace_noDuplicateCheck(str, &completer);
}

void GlobalCommandController::unregisterCompleter(
	CommandCompleter& completer, string_view str)
{
	if (StringOp::startsWith(str, "::")) str.remove_prefix(2); // drop leading ::
	assert(commandCompleters.contains(str));
	assert(commandCompleters[str] == &completer); (void)completer;
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

static vector<string> split(string_view str, const char delimiter)
{
	vector<string> tokens;

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
	return tokens;
}

static string removeEscaping(const string& str)
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

static vector<string> removeEscaping(span<const string> input, bool keepLastIfEmpty)
{
	vector<string> result;
	for (const auto& s : input) {
		if (!s.empty()) {
			result.push_back(removeEscaping(s));
		}
	}
	if (keepLastIfEmpty && (input.empty() || input.back().empty())) {
		result.emplace_back();
	}
	return result;
}

static string escapeChars(const string& str, string_view chars)
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

static string addEscaping(const string& str, bool quote, bool finished)
{
	if (str.empty() && finished) {
		quote = true;
	}
	string result = escapeChars(str, "$[]");
	if (quote) {
		result.insert(result.begin(), '"');
		if (finished) {
			result += '"';
		}
	} else {
		result = escapeChars(result, " ");
	}
	return result;
}

bool GlobalCommandController::isComplete(zstring_view command)
{
	return interpreter.isComplete(command);
}

TclObject GlobalCommandController::executeCommand(
	zstring_view command, CliConnection* connection_)
{
	ScopedAssign sa(connection, connection_);
	return interpreter.execute(command);
}

void GlobalCommandController::source(const string& script)
{
	try {
		LocalFileReference file(script);
		interpreter.executeFile(file.getFilename());
	} catch (CommandException& e) {
		getCliComm().printWarning(
			 "While executing ", script, ": ", e.getMessage());
	}
}

string GlobalCommandController::tabCompletion(string_view command)
{
	// split on 'active' command (the command that should actually be
	// completed). Some examples:
	//    if {[debug rea<tab> <-- should complete the 'debug' command
	//                              instead of the 'if' command
	//    bind F6 { cycl<tab> <-- should complete 'cycle' instead of 'bind'
	TclParser parser = interpreter.parse(command);
	int last = parser.getLast();
	string_view pre  = command.substr(0, last);
	string_view post = command.substr(last);

	// split command string in tokens
	vector<string> originalTokens = split(post, ' ');
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
	return strCat(pre, join(originalTokens, ' '));
}

void GlobalCommandController::tabCompletion(vector<string>& tokens)
{
	if (tokens.empty()) {
		// nothing typed yet
		return;
	}
	if (tokens.size() == 1) {
		string_view cmd = tokens[0];
		string_view leadingNs;
		// remove leading ::
		if (StringOp::startsWith(cmd, "::")) {
			cmd.remove_prefix(2);
			leadingNs = "::";
		}
		// get current (typed) namespace
		auto p1 = cmd.rfind("::");
		string_view ns = (p1 == string_view::npos) ? cmd : cmd.substr(0, p1 + 2);

		// build a list of all command strings
		TclObject names = interpreter.getCommandNames();
		vector<string> names2;
		names2.reserve(names.size());
		for (string_view n1 : names) {
			// remove leading ::
			if (StringOp::startsWith(n1, "::")) n1.remove_prefix(2);
			// initial namespace part must match
			if (!StringOp::startsWith(n1, ns)) continue;
			// the part following the initial namespace
			string_view n2 = n1.substr(ns.size());
			// only keep upto the next namespace portion,
			auto p2 = n2.find("::");
			auto n3 = (p2 == string_view::npos) ? n1 : n1.substr(0, ns.size() + p2 + 2);
			// don't care about adding the same string multiple times
			names2.push_back(strCat(leadingNs, n3));
		}
		Completer::completeString(tokens, names2);
	} else {
		string_view cmd = tokens.front();
		if (StringOp::startsWith(cmd, "::")) cmd.remove_prefix(2); // drop leading ::

		if (auto* v = lookup(commandCompleters, cmd)) {
			(*v)->tabCompletion(tokens);
		} else {
			TclObject command = makeTclList("openmsx::tabcompletion");
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
					"proc: ", e.getMessage());
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
	span<const TclObject> tokens, TclObject& result)
{
	auto& controller = OUTER(GlobalCommandController, helpCmd);
	switch (tokens.size()) {
	case 1: {
		string text =
			"Use 'help [command]' to get help for a specific command\n"
			"The following commands exist:\n";
		auto cmds = concat<string_view>(
			view::keys(controller.commandCompleters),
			getInterpreter().execute("openmsx::all_command_names_with_help"));
		cmds.erase(ranges::remove_if(cmds, [](const auto& c) {
		                   return c.find("::") != std::string_view::npos; }),
		           cmds.end());
		ranges::sort(cmds);
		for (auto& line : formatListInColumns(cmds)) {
			strAppend(text, line, '\n');
		}
		result = text;
		break;
	}
	default: {
		if (const auto* v = lookup(controller.commandCompleters, tokens[1].getString())) {
			result = (*v)->help(tokens.subspan(1));
		} else {
			TclObject command = makeTclList("openmsx::help");
			command.addListElements(view::drop(tokens, 1));
			result = command.executeCommand(getInterpreter());
		}
		break;
	}
	}
}

string GlobalCommandController::HelpCmd::help(span<const TclObject> /*tokens*/) const
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
	span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 2, "commandstring");
	// TODO this prints list of possible completions in the console
	auto& controller = OUTER(GlobalCommandController, tabCompletionCmd);
	result = controller.tabCompletion(tokens[1].getString());
}

string GlobalCommandController::TabCompletionCmd::help(span<const TclObject> /*tokens*/) const
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
	throw CommandException("No such update type: ", name.getString());
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
	span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 3, Prefix{1}, "enable|disable type");
	if (tokens[1] == "enable") {
		getConnection().setUpdateEnable(getType(tokens[2]), true);
	} else if (tokens[1] == "disable") {
		getConnection().setUpdateEnable(getType(tokens[2]), false);
	} else {
		throw SyntaxError();
	}
}

string GlobalCommandController::UpdateCmd::help(span<const TclObject> /*tokens*/) const
{
	return "Enable or disable update events for external applications. See doc/openmsx-control-xml.txt.";
}

void GlobalCommandController::UpdateCmd::tabCompletion(vector<string>& tokens) const
{
	switch (tokens.size()) {
	case 2: {
		using namespace std::literals;
		static constexpr std::array ops = {"enable"sv, "disable"sv};
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
	span<const TclObject> /*tokens*/, TclObject& result) const
{
	result = TARGET_PLATFORM;
}

string GlobalCommandController::PlatformInfo::help(span<const TclObject> /*tokens*/) const
{
	return "Prints openMSX platform.";
}

// Version info

GlobalCommandController::VersionInfo::VersionInfo(InfoCommand& openMSXInfoCommand_)
	: InfoTopic(openMSXInfoCommand_, "version")
{
}

void GlobalCommandController::VersionInfo::execute(
	span<const TclObject> /*tokens*/, TclObject& result) const
{
	result = Version::full();
}

string GlobalCommandController::VersionInfo::help(span<const TclObject> /*tokens*/) const
{
	return "Prints openMSX version.";
}

} // namespace openmsx

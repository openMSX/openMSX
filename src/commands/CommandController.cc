// $Id$

#include <cassert>
#include <cstdlib>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "CommandController.hh"
#include "CommandConsole.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "File.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "openmsx.hh"
#include "CliCommOutput.hh"
#include "Interpreter.hh"
#include "InfoCommand.hh"


namespace openmsx {

CommandController::CommandController()
	: helpCmd(*this), cmdConsole(NULL),
	  infoCommand(InfoCommand::instance()),
	  settingsConfig(SettingsConfig::instance()),
	  output(CliCommOutput::instance()),
	  interpreter(Interpreter::instance())
{
	registerCommand(&helpCmd, "help");
	registerCommand(&infoCommand, "openmsx_info");
}

CommandController::~CommandController()
{
	unregisterCommand(&infoCommand, "openmsx_info");
	unregisterCommand(&helpCmd, "help");
	assert(commands.empty());
	assert(commandCompleters.empty());
	assert(!cmdConsole);
}

CommandController& CommandController::instance()
{
	static CommandController oneInstance;
	return oneInstance;
}


void CommandController::registerCommand(Command* command,
                                        const string& str)
{
	assert(commands.find(str) == commands.end());

	registerCompleter(command, str);
	commands[str] = command;
	interpreter.registerCommand(str, *command);
}

void CommandController::unregisterCommand(Command* command,
                                          const string& str)
{
	assert(commands.find(str) != commands.end());
	assert(commands.find(str)->second == command);

	interpreter.unregisterCommand(str, *command);
	commands.erase(str);
	unregisterCompleter(command, str);
}

void CommandController::registerCompleter(CommandCompleter* completer,
                                          const string& str)
{
	assert(commandCompleters.find(str) == commandCompleters.end());
	commandCompleters[str] = completer;
}

void CommandController::unregisterCompleter(CommandCompleter* completer,
                                            const string& str)
{
	assert(commandCompleters.find(str) != commandCompleters.end());
	assert(commandCompleters.find(str)->second == completer);
	commandCompleters.erase(str);
}


bool CommandController::hasCommand(const string& command)
{
	return commands.find(command) != commands.end();
}

void CommandController::split(const string& str, vector<string>& tokens,
                              const char delimiter)
{
	enum ParseState {Alpha, BackSlash, Quote};
	ParseState state = Alpha;

	for (unsigned i = 0; i < str.length(); ++i) {
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

string CommandController::removeEscaping(const string& str)
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

void CommandController::removeEscaping(const vector<string>& input,
                              vector<string>& output, bool keepLastIfEmpty)
{
	for (vector<string>::const_iterator it = input.begin();
	     it != input.end();
	     ++it) {
		if (!it->empty()) {
			output.push_back(removeEscaping(*it));
		}
	}
	if (keepLastIfEmpty && (input.empty() || input.back().empty())) {
		output.push_back("");
	}
}

string CommandController::addEscaping(const string& str, bool quote, bool finished)
{
	if (str.empty() && finished) {
		quote = true;
	}
	string result;
	if (quote) {
		result = '"' + str;
		if (finished) {
			result += '"';
		}
	} else {
		for (unsigned i = 0; i < str.length(); ++i) {
			char chr = str[i];
			if (chr == ' ') {
				result += '\\';
			}
			result += chr;
		}
	}
	return result;
}

string CommandController::join(const vector<string>& tokens, char delimiter)
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

bool CommandController::isComplete(const string& command) const
{
	return interpreter.isComplete(command);
}

string CommandController::executeCommand(const string& cmd)
	throw (CommandException)
{
	return interpreter.execute(cmd);
}

void CommandController::autoCommands()
{
	try {
		SystemFileContext context(true); // only in system dir
		File file(context.resolve("init.tcl"));
		interpreter.executeFile(file.getLocalName());
	} catch (FileException& e) {
		// no init.tcl
	} catch (CommandException& e) {
		// TODO
	}
	
	try {
		Config* config = settingsConfig.getConfigById("AutoCommands");
		output.printWarning(
			"Use of AutoCommands is deprecated, instead use the init.tcl script.\n"
			"See manual for more information.");

		Config::Parameters commands;
		config->getParametersWithClass("", commands);
		for (Config::Parameters::const_iterator it = commands.begin();
		     it != commands.end(); ++it) {
			try {
				executeCommand(it->second);
			} catch (CommandException &e) {
				output.printWarning(
				         "While executing autocommands: "
				         + e.getMessage());
			}
		}
	} catch (ConfigException &e) {
		// no auto commands defined
	}
}

void CommandController::setCommandConsole(CommandConsole* console)
{
	cmdConsole = console;
}

void CommandController::tabCompletion(string &command)
{
	// split in sub commands
	vector<string> subcmds;
	split(command, subcmds, ';');
	if (subcmds.empty()) {
		subcmds.push_back("");
	}
	
	// split command string in tokens
	vector<string> originalTokens;
	split(subcmds.back(), originalTokens, ' ');
	if (originalTokens.empty()) {
		originalTokens.push_back("");
	}
	
	// complete last token
	vector<string> tokens;
	removeEscaping(originalTokens, tokens, true);
	unsigned oldNum = tokens.size();
	tabCompletion(tokens);
	unsigned newNum = tokens.size();
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
	subcmds.back() = join(originalTokens, ' ');
	command = join(subcmds, ';');
}

void CommandController::tabCompletion(vector<string> &tokens)
{
	if (tokens.empty()) {
		// nothing typed yet
		return;
	}
	if (tokens.size() == 1) {
		// build a list of all command strings
		set<string> cmds;
		interpreter.getCommandNames(cmds);
		completeString(tokens, cmds);
	} else {
		CompleterMap::const_iterator it = commandCompleters.find(tokens.front());
		if (it != commandCompleters.end()) {
			it->second->tabCompletion(tokens);
		}
	}
}

bool CommandController::equal(const string& s1, const string& s2,
                              bool caseSensitive)
{
	if (caseSensitive) {
		return s1 == s2;
	} else {
		return strcasecmp(s1.c_str(), s2.c_str()) == 0;
	}
}

bool CommandController::completeString2(string &str, set<string>& st,
                                        bool caseSensitive)
{
	CommandConsole* cmdConsole = CommandController::instance().cmdConsole;
	assert(cmdConsole);
	set<string>::iterator it = st.begin();
	while (it != st.end()) {
		if (equal(str, (*it).substr(0, str.size()), caseSensitive)) {
			++it;
		} else {
			set<string>::iterator it2 = it;
			++it;
			st.erase(it2);
		}
	}
	if (st.empty()) {
		// no matching commands
		return false;
	}
	if (st.size() == 1) {
		// only one match
		str = *(st.begin());
		return true;
	}
	bool expanded = false;
	while (true) {
		it = st.begin();
		if (equal(str, *it, caseSensitive)) {
			// match is as long as first word
			goto out;	// TODO rewrite this
		}
		// expand with one char and check all strings 
		string string2 = (*it).substr(0, str.size() + 1);
		for (;  it != st.end(); it++) {
			if (!equal(string2, (*it).substr(0, string2.size()),
				   caseSensitive)) {
				goto out;	// TODO rewrite this
			}
		}
		// no conflict found
		str = string2;
		expanded = true;
	}
	out:
	if (!expanded) {
		// print all possibilities
		for (it = st.begin(); it != st.end(); ++it) {
			// TODO print more on one line
			cmdConsole->printFast(*it);
		}
		cmdConsole->printFlush();
	}
	return false;
}
void CommandController::completeString(vector<string> &tokens,
                                       set<string>& st,
                                       bool caseSensitive)
{
	if (completeString2(tokens.back(), st, caseSensitive)) {
		tokens.push_back("");
	}
}

void CommandController::completeFileName(vector<string> &tokens)
{
	string& filename = tokens[tokens.size() - 1];
	filename = FileOperations::expandTilde(filename);
	string basename = FileOperations::getBaseName(filename);
	vector<string> paths;
	if (FileOperations::isAbsolutePath(filename)) {
		// absolute path, only try root directory
		paths.push_back("");
	} else {
		// relative path, also try user directories
		UserFileContext context;
		paths = context.getPaths();
	}
	
	set<string> filenames;
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end();
	     ++it) {
		string dirname = *it + basename;
		DIR* dirp = opendir(FileOperations::getNativePath(dirname).c_str());
		if (dirp != NULL) {
			while (dirent* de = readdir(dirp)) {
				struct stat st;
				string name = dirname + de->d_name;
				if (!(stat(FileOperations::getNativePath(name).c_str(), &st))) {
					string nm = basename + de->d_name;
					if (S_ISDIR(st.st_mode)) {
						nm += "/";
					}
					filenames.insert(FileOperations::getConventionalPath(nm));
				}
			}
			closedir(dirp);
		}
	}
	bool t = completeString2(filename, filenames, true);
	if (t && !filename.empty() && filename[filename.size() - 1] != '/') {
		// completed filename, start new token
		tokens.push_back("");
	}
}

// Help Command

CommandController::HelpCmd::HelpCmd(CommandController& parent_)
	: parent(parent_)
{
}

string CommandController::HelpCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	switch (tokens.size()) {
	case 1: 
		result += "Use 'help [command]' to get help for a specific command\n";
		result += "The following commands exist:\n";
		for (CommandMap::const_iterator it = parent.commands.begin();
		     it != parent.commands.end(); ++it) {
			result += it->first;
			result += '\n';
		}
		break;
	default: {
		CommandMap::iterator it = parent.commands.find(tokens[1]);
		if (it == parent.commands.end()) {
			throw CommandException(tokens[1] + ": unknown command");
		}
		vector<string>::const_iterator remainder = tokens.begin();
		vector<string> tokens2(++remainder, tokens.end());
		result += it->second->help(tokens2);
		break;
	}
	}
	return result;
}
string CommandController::HelpCmd::help(const vector<string> &tokens) const
	throw()
{
	return "prints help information for commands\n";
}
void CommandController::HelpCmd::tabCompletion(vector<string> &tokens) const
	throw()
{
	string front = tokens.front();
	tokens.erase(tokens.begin());
	parent.tabCompletion(tokens);
	tokens.insert(tokens.begin(), front);
}

} // namespace openmsx

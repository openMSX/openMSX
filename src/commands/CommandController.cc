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
#include "MSXConfig.hh"
#include "Config.hh"
#include "openmsx.hh"
#include "CliCommunicator.hh"
#include "InfoCommand.hh"


namespace openmsx {

CommandController::CommandController()
	: aliasCmds(*this)
{
	registerCommand(&helpCmd, "help");
	registerCommand(&InfoCommand::instance(), "info");
}

CommandController::~CommandController()
{
	unregisterCommand(&InfoCommand::instance(), "info");
	unregisterCommand(&helpCmd, "help");
	assert(commands.empty());
}

CommandController *CommandController::instance()
{
	// TODO mem leak
	static CommandController* oneInstance = NULL;
	if (oneInstance == NULL) {
		oneInstance = new CommandController();
	}
	return oneInstance;
}


void CommandController::registerCommand(Command* command,
                                        const string& str)
{
	assert(commands.find(str) == commands.end());
	commands[str] = command;
}

void CommandController::unregisterCommand(Command* command,
                                          const string& str)
{
	assert(commands.find(str) != commands.end());
	assert(commands.find(str)->second == command);
	commands.erase(str);
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


string CommandController::executeCommand(const string &cmd)
{
	vector<string> subcmds;
	split(cmd, subcmds, ';');

	string result;
	for (vector<string>::const_iterator it = subcmds.begin();
	     it != subcmds.end();
	     ++it) {
		const string& command = *it;
		vector<string> originalTokens;
		split(command, originalTokens, ' ');
		
		vector<string> tokens;
		removeEscaping(originalTokens, tokens, false);
		if (tokens.empty()) {
			continue;
		}

		CommandMap::iterator it = commands.find(tokens.front());
		if (it == commands.end()) {
			throw CommandException(tokens.front() + ": unknown command");
		}
		result += it->second->execute(tokens);
	}
	return result;
}

void CommandController::autoCommands()
{
	try {
		Config* config = MSXConfig::instance()->getConfigById("AutoCommands");
		list<Config::Parameter*>* commandList;
		commandList = config->getParametersWithClass("");
		for (list<Config::Parameter*>::const_iterator i = commandList->begin();
		     i != commandList->end(); ++i) {
			try {
				executeCommand((*i)->value);
			} catch (CommandException &e) {
				CliCommunicator::instance().printWarning(
				         "While executing autocommands: "
				         + e.getMessage());
			}
		}
		config->getParametersWithClassClean(commandList);
	} catch (ConfigException &e) {
		// no auto commands defined
	}
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
		for (CommandMap::const_iterator it = commands.begin();
		     it != commands.end(); it++) {
			cmds.insert(it->first);
		}
		completeString(tokens, cmds);
	} else {
		CommandMap::const_iterator it = commands.find(tokens.front());
		if (it != commands.end()) {
			it->second->tabCompletion(tokens);
		}
	}
}

bool CommandController::completeString2(string &str, set<string>& st)
{
	set<string>::iterator it = st.begin();
	while (it != st.end()) {
		if (str == (*it).substr(0, str.size())) {
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
		if (str == *it) {
			// match is as long as first word
			goto out;	// TODO rewrite this
		}
		// expand with one char and check all strings 
		string string2 = str + (*it)[str.size()];
		for (;  it != st.end(); it++) {
			if (string2 != (*it).substr(0, string2.size())) {
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
			CommandConsole::instance()->printFast(*it);
		}
		CommandConsole::instance()->printFlush();
	}
	return false;
}
void CommandController::completeString(vector<string> &tokens,
                                       set<string>& st)
{
	if (completeString2(tokens.back(), st)) {
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
	bool t = completeString2(filename, filenames);
	if (t && !filename.empty() && filename[filename.size() - 1] != '/') {
		// completed filename, start new token
		tokens.push_back("");
	}
}

// Help Command

string CommandController::HelpCmd::execute(const vector<string> &tokens)
	throw(CommandException)
{
	string result;
	CommandController *cc = CommandController::instance();
	switch (tokens.size()) {
	case 1: 
		result += "Use 'help [command]' to get help for a specific command\n";
		result += "The following commands exist:\n";
		for (CommandMap::const_iterator it = cc->commands.begin();
		     it != cc->commands.end(); ++it) {
			result += it->first;
			result += '\n';
		}
		break;
	default: {
		CommandMap::iterator it = cc->commands.find(tokens[1]);
		if (it == cc->commands.end()) {
			throw CommandException(tokens[1] + ": unknown command");
		}
		vector<string> tokens2(++tokens.begin(), tokens.end());
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
	CommandController::instance()->tabCompletion(tokens);
	tokens.insert(tokens.begin(), front);
}

} // namespace openmsx

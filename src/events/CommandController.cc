// $Id$

#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include "openmsx.hh"
#include "MSXConfig.hh"
#include "CommandController.hh"
#include "CommandConsole.hh"
#include "FileOperations.hh"


CommandController::CommandController()
{
	registerCommand(&helpCmd, "help");
}

CommandController::~CommandController()
{
	unregisterCommand(&helpCmd, "help");
}

CommandController *CommandController::instance()
{
	static CommandController* oneInstance = NULL;
	if (oneInstance == NULL) {
		oneInstance = new CommandController();
	}
	return oneInstance;
}


void CommandController::registerCommand(Command *command, 
                                        const string &str)
{
	commands.insert(pair<string, Command*>(str, command));
}

void CommandController::unregisterCommand(Command *command,
                                          const string &str)
{
	multimap<string, Command*>::iterator it;
	for (it = commands.lower_bound(str);
	     (it != commands.end()) && (it->first == str);
	     it++) {
		if (it->second == command) {
			commands.erase(it);
			break;
		}
	}
}


void CommandController::tokenize(const string &str,
                                 vector<string> &tokens,
                                 const string &delimiters)
{
	enum ParseState {Alpha, BackSlash, Quote, Space};
	ParseState state = Space;
	tokens.push_back(string());
	for (unsigned i=0; i<str.length(); i++) {
		char chr = str[i];
		switch (state) {
			case Space:
				if (delimiters.find(chr) != string::npos) {
					// nothing
				} else {
					if (chr == '\\') {
						state = BackSlash;
					} else if (chr == '"') {
						state = Quote;
					} else {
						tokens[tokens.size()-1] += chr;
						state = Alpha;
					}
				}
				break;
			case Alpha:
				if (delimiters.find(chr) != string::npos) {
					// token done, start new token
					tokens.push_back(string());
					state = Space;
				} else if (chr == '\\') {
					state = BackSlash;
				} else if (chr == '"') {
					state = Quote;
				} else {
					tokens[tokens.size()-1] += chr;
				}
				break;
			case Quote:
				if (chr == '"') {
					state = Alpha;
				} else {
					tokens[tokens.size()-1] += chr;
				}
				break;
			case BackSlash:
				tokens[tokens.size()-1] += chr;
				state = Alpha;
				break;
		}
	}
	if ((tokens.size() > 1) && (tokens[0] == ""))
		tokens.erase(tokens.begin());
}

void CommandController::executeCommand(const string &cmd)
{
	vector<string> tokens;
	tokenize(cmd, tokens);
	if (tokens[tokens.size()-1] == "")
		tokens.resize(tokens.size()-1);
	if (tokens.empty())
		return;
	multimap<const string, Command*, ltstr>::const_iterator it;
	it = commands.lower_bound(tokens[0]);
	if (it == commands.end() || it->first != tokens[0]) {
		throw CommandException("Unknown command");
	}
	while (it != commands.end() && it->first == tokens[0]) {
		it->second->execute(tokens);
		it++;
	}
}

void CommandController::autoCommands()
{
	try {
		Config *config = MSXConfig::instance()->getConfigById("AutoCommands");
		list<Device::Parameter*>* commandList;
		commandList = config->getParametersWithClass("");
		list<Device::Parameter*>::const_iterator i;
		for (i = commandList->begin(); i != commandList->end(); i++) {
			try {
				executeCommand((*i)->value);
			} catch (CommandException &e) {
				PRT_INFO("Warning, while executing autocommands:\n"
				         "   " << e.getMessage());
			}
		}
		config->getParametersWithClassClean(commandList);
	} catch (ConfigException &e) {
		// no auto commands defined
	}
}


void CommandController::tabCompletion(string &command)
{
	// split command string in tokens
	vector<string> tokens;
	tokenize(command, tokens);
	
	// complete last token
	tabCompletion(tokens);
	
	// rebuild command string from tokens
	command = string("");
	vector<string>::const_iterator it;
	for (it=tokens.begin(); it!=tokens.end(); it++) {
		if (it != tokens.begin())
			command += ' ';
		if (it->find(' ') == string::npos) {
			command += *it;
		} else {
			command += '"';
			command += *it;
			command += '"';
		}
	}
}

void CommandController::tabCompletion(vector<string> &tokens)
{
	if (tokens.empty()) {
		// nothing typed yet
		return;
	}
	if (tokens.size()==1) {
		// build a list of all command strings
		set<string> cmds;
		multimap<const string, Command*, ltstr>::const_iterator it;
		for (it = commands.begin(); it != commands.end(); it++) {
			cmds.insert(it->first);
		}
		completeString(tokens, cmds);
	} else {
		multimap<const string, Command*, ltstr>::const_iterator it;
		it = commands.find(tokens[0]);	// just take one
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
		CommandConsole::instance()->printFast("");	// dummy 
		CommandConsole::instance()->printFlush();	// dummy 
	}
	return false;
}
void CommandController::completeString(vector<string> &tokens,
                                       set<string>& st)
{
	if (completeString2(tokens[tokens.size()-1], st))
		tokens.push_back(string());
}

void CommandController::completeFileName(vector<string> &tokens)
{
	string& filename = tokens[tokens.size()-1];
	filename = FileOperations::expandTilde(filename);
	string basename = FileOperations::getBaseName(filename);
	vector<string> paths;
	if (!filename.empty() && (filename[0] == '/')) {
		// absolute path
		paths.push_back("/");
	} else {
		// realtive path, also try user directories
		UserFileContext context;
		paths = context.getPaths();
	}
	
	set<string> filenames;
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end();
	     ++it) {
		string dirname = *it + basename;
		DIR* dirp = opendir(dirname.c_str());
		if (dirp != NULL) {
			while (dirent* de = readdir(dirp)) {
				struct stat st;
				string name = dirname + de->d_name;
				if (!(stat(name.c_str(), &st))) {
					string nm = basename + de->d_name;
					if (S_ISDIR(st.st_mode)) {
						nm += "/";
					}
					filenames.insert(nm);
				}
			}
			closedir(dirp);
		}
	}
	bool t = completeString2(filename, filenames);
	if (t && filename[filename.size()-1] != '/') {
		// completed filename, start new token
		tokens.push_back(string());
	}
}

// Help Command

void CommandController::HelpCmd::execute(const vector<string> &tokens)
{
	CommandController *cc = CommandController::instance();
	switch (tokens.size()) {
		case 1: {
			print("Use 'help [command]' to get help for a specific command");
			print("The following commands exist:");
			multimap<const string, Command*, ltstr>::const_iterator it;
			for (it=cc->commands.begin(); it!=cc->commands.end(); it++) {
				print(it->first);
			}
			break;
		}
		default: {
			multimap<const string, Command*, ltstr>::const_iterator it;
			it = cc->commands.lower_bound(tokens[1]);
			if (it == cc->commands.end() || it->first != tokens[1])
				throw CommandException("Unknown command");
			while (it != cc->commands.end() && it->first == tokens[1]) {
				vector<string> tokens2(tokens);
				tokens2.erase(tokens2.begin());
				it->second->help(tokens2);
				it++;
			}
			break;
		}
	}
}
void CommandController::HelpCmd::help(const vector<string> &tokens) const
{
	print("prints help information for commands");
}
void CommandController::HelpCmd::tabCompletion(vector<string> &tokens) const
{
	string front = tokens.front();
	tokens.erase(tokens.begin());
	CommandController::instance()->tabCompletion(tokens);
	tokens.insert(tokens.begin(), front);
}

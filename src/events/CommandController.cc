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
#include "Console.hh"
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
                                        const std::string &str)
{
	commands.insert(std::pair<std::string, Command*>(str, command));
}

void CommandController::unregisterCommand(Command *command,
                                          const std::string &str)
{
	std::multimap<std::string, Command*>::iterator it;
	for (it = commands.lower_bound(str);
	     (it != commands.end()) && (it->first == str);
	     it++) {
		if (it->second == command) {
			commands.erase(it);
			break;
		}
	}
}


void CommandController::tokenize(const std::string &str,
                                 std::vector<std::string> &tokens,
                                 const std::string &delimiters)
{
	enum ParseState {Alpha, BackSlash, Quote, Space};
	ParseState state = Space;
	tokens.push_back(std::string());
	for (unsigned i=0; i<str.length(); i++) {
		char chr = str[i];
		switch (state) {
			case Space:
				if (delimiters.find(chr) != std::string::npos) {
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
				if (delimiters.find(chr) != std::string::npos) {
					// token done, start new token
					tokens.push_back(std::string());
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

void CommandController::executeCommand(const std::string &cmd)
{
	std::vector<std::string> tokens;
	tokenize(cmd, tokens);
	if (tokens[tokens.size()-1] == "")
		tokens.resize(tokens.size()-1);
	if (tokens.empty())
		return;
	std::multimap<const std::string, Command*, ltstr>::const_iterator it;
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
		std::list<Device::Parameter*>* commandList;
		commandList = config->getParametersWithClass("");
		std::list<Device::Parameter*>::const_iterator i;
		for (i = commandList->begin(); i != commandList->end(); i++) {
			try {
				executeCommand((*i)->value);
			} catch (CommandException &e) {
				PRT_INFO("Warning, while executing autocommands:\n"
				         "   " << e.getMessage());
			}
		}
	} catch (ConfigException &e) {
		// no auto commands defined
	}
}


void CommandController::tabCompletion(std::string &command)
{
	// split command string in tokens
	std::vector<std::string> tokens;
	tokenize(command, tokens);
	
	// complete last token
	tabCompletion(tokens);
	
	// rebuild command string from tokens
	command = std::string("");
	std::vector<std::string>::const_iterator it;
	for (it=tokens.begin(); it!=tokens.end(); it++) {
		if (it != tokens.begin())
			command += ' ';
		if (it->find(' ') == std::string::npos) {
			command += *it;
		} else {
			command += '"';
			command += *it;
			command += '"';
		}
	}
}

void CommandController::tabCompletion(std::vector<std::string> &tokens)
{
	if (tokens.empty()) {
		// nothing typed yet
		return;
	}
	if (tokens.size()==1) {
		// build a list of all command strings
		std::set<std::string> cmds;
		std::multimap<const std::string, Command*, ltstr>::const_iterator it;
		for (it = commands.begin(); it != commands.end(); it++) {
			cmds.insert(it->first);
		}
		completeString(tokens, cmds);
	} else {
		std::multimap<const std::string, Command*, ltstr>::const_iterator it;
		it = commands.find(tokens[0]);	// just take one
		if (it != commands.end()) {
			it->second->tabCompletion(tokens);
		}
	}
}

bool CommandController::completeString2(std::string &string,
                                        std::set<std::string> &set)
{
	std::set<std::string>::iterator it = set.begin();
	while (it != set.end()) {
		if (string == (*it).substr(0, string.size())) {
			it++;
		} else {
			std::set<std::string>::iterator it2 = it;
			it++;
			set.erase(it2);
		}
	}
	if (set.empty()) {
		// no matching commands
		return false;
	}
	if (set.size()==1) {
		// only one match
		string = *(set.begin());
		return true;
	}
	bool expanded = false;
	while (true) {
		it = set.begin();
		if (string == *it) {
			// match is as long as first word
			goto out;	// TODO rewrite this
		}
		// expand with one char and check all strings 
		std::string string2 = string + (*it)[string.size()];
		for (;  it != set.end(); it++) {
			if (string2 != (*it).substr(0, string2.size())) {
				goto out;	// TODO rewrite this
			}
		}
		// no conflict found
		string = string2;
		expanded = true;
	}
	out:
	if (!expanded) {
		// print all possibilities
		for (it = set.begin(); it != set.end(); it++) {
			// TODO print more on one line
			Console::instance()->printFast(*it);
		}
		Console::instance()->printFast("");	// dummy 
		Console::instance()->printFlush();	// dummy 
	}
	return false;
}
void CommandController::completeString(std::vector<std::string> &tokens,
                                       std::set<std::string> &set)
{
	if (completeString2(tokens[tokens.size()-1], set))
		tokens.push_back(std::string());
}

void CommandController::completeFileName(std::vector<std::string> &tokens)
{
	std::string& filename = tokens[tokens.size()-1];
	filename = FileOperations::expandTilde(filename);
	std::string basename = FileOperations::getBaseName(filename);
	std::list<std::string> paths;
	if (!filename.empty() && (filename[0] == '/')) {
		// absolute path
		paths.push_back("/");
	} else {
		// realtive path, also try user directories
		UserFileContext context;
		paths = context.getPaths();
	}
	
	std::set<std::string> filenames;
	for (std::list<std::string>::const_iterator it = paths.begin();
	     it != paths.end();
	     it++) {
		std::string dirname = *it + basename;
		DIR* dirp = opendir(dirname.c_str());
		if (dirp != NULL) {
			while (dirent* de = readdir(dirp)) {
				struct stat st;
				std::string name = dirname + de->d_name;
				if (!(stat(name.c_str(), &st))) {
					std::string nm = basename + de->d_name;
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
		tokens.push_back(std::string());
	}
}

// Help Command

void CommandController::HelpCmd::execute(const std::vector<std::string> &tokens)
{
	CommandController *cc = CommandController::instance();
	switch (tokens.size()) {
		case 1: {
			print("Use 'help [command]' to get help for a specific command");
			print("The following commands exist:");
			std::multimap<const std::string, Command*, ltstr>::const_iterator it;
			for (it=cc->commands.begin(); it!=cc->commands.end(); it++) {
				print(it->first);
			}
			break;
		}
		default: {
			std::multimap<const std::string, Command*, ltstr>::const_iterator it;
			it = cc->commands.lower_bound(tokens[1]);
			if (it == cc->commands.end() || it->first != tokens[1])
				throw CommandException("Unknown command");
			while (it != cc->commands.end() && it->first == tokens[1]) {
				std::vector<std::string> tokens2(tokens);
				tokens2.erase(tokens2.begin());
				it->second->help(tokens2);
				it++;
			}
			break;
		}
	}
}
void CommandController::HelpCmd::help(const std::vector<std::string> &tokens) const
{
	print("prints help information for commands");
}
void CommandController::HelpCmd::tabCompletion(std::vector<std::string> &tokens) const
{
	std::string front = tokens.front();
	tokens.erase(tokens.begin());
	CommandController::instance()->tabCompletion(tokens);
	tokens.insert(tokens.begin(), front);
}


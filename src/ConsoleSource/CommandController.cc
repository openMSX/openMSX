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
#include "ConsoleManager.hh"


CommandController::CommandController()
{
	registerCommand(helpCmd, "help");
}

CommandController::~CommandController()
{
	unregisterCommand("help");
}

CommandController *CommandController::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new CommandController();
	}
	return oneInstance;
}
CommandController *CommandController::oneInstance = NULL;


void CommandController::registerCommand(Command &command, const std::string &str)
{
	commands[str] = &command;
}

void CommandController::unregisterCommand(const std::string &str)
{
	commands.erase(str);
}


void CommandController::tokenize(const std::string &str, std::vector<std::string> &tokens, const std::string &delimiters)
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
	std::map<const std::string, Command*, ltstr>::const_iterator it;
	it = commands.find(tokens[0]);
	if (it==commands.end()) {
		throw CommandException("Unknown command");
	} else {
		it->second->execute(tokens);
	}
}

void CommandController::autoCommands()
{
	try {
		MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("AutoCommands");
		std::list<MSXConfig::Device::Parameter*>* commandList;
		commandList = config->getParametersWithClass("");
		std::list<MSXConfig::Device::Parameter*>::const_iterator i;
		for (i = commandList->begin(); i != commandList->end(); i++) {
			executeCommand((*i)->value);
		}
	} catch (MSXConfig::Exception &e) {
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
		std::list<std::string> cmds;
		std::map<const std::string, Command*, ltstr>::const_iterator it;
		for (it=commands.begin(); it!=commands.end(); it++) {
			cmds.push_back(it->first);
		}
		completeString(tokens, cmds);
	} else {
		std::map<const std::string, Command*, ltstr>::const_iterator it;
		it = commands.find(tokens[0]);
		if (it!=commands.end()) {
			it->second->tabCompletion(tokens);
		}
	}
}

bool CommandController::completeString2(std::string &string, std::list<std::string> &list)
{
	std::list<std::string>::iterator it;
	
	it = list.begin();
	while (it != list.end()) {
		if (string == (*it).substr(0, string.size())) {
			it++;
		} else {
			std::list<std::string>::iterator it2 = it;
			it++;
			list.erase(it2);
		}
	}
	if (list.empty()) {
		// no matching commands
		return false;
	}
	if (list.size()==1) {
		// only one match
		string = *(list.begin());
		return true;
	}
	bool expanded = false;
	while (true) {
		it = list.begin();
		if (string == *it) {
			// match is as long as first word
			goto out;	// TODO rewrite this
		}
		// expand with one char and check all strings 
		std::string string2 = string + (*it)[string.size()];
		for (;  it!=list.end(); it++) {
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
		for (it = list.begin(); it != list.end(); it++) {
			// TODO print more on one line
			ConsoleManager::instance()->print(*it);
		}
		ConsoleManager::instance()->print("");	// dummy 
	}
	return false;
}
void CommandController::completeString(std::vector<std::string> &tokens, std::list<std::string> &list)
{
	if (completeString2(tokens[tokens.size()-1], list))
		tokens.push_back(std::string());
}

void CommandController::completeFileName(std::vector<std::string> &tokens)
{
	std::string& filename = tokens[tokens.size()-1];
	std::string npath, dpath;
	std::list<std::string> filenames;
	std::string::size_type pos = filename.find_last_of("/");	// TODO std delimiter
	if (pos == std::string::npos) {
		dpath = ".";
		npath = "";
	} else {
		dpath = filename.substr(0, pos ? pos : 1);	// excl "/" except for root
		npath = filename.substr(0, pos+1);	// incl "/"
	}
	
	DIR* dirp = opendir(dpath.c_str());
	if (dirp != NULL) {
		while (dirent* de = readdir(dirp)) {
			struct stat st;
			if (!(stat((npath + de->d_name).c_str(), &st))) {
				std::string name = npath + de->d_name;
				if (S_ISDIR(st.st_mode))
					name += "/";
				filenames.push_back(name);
			}
		}
	}
	bool t = completeString2(filename, filenames);
	if (t && filename[filename.size()-1] != '/')
		tokens.push_back(std::string());
}

// Help Command

void CommandController::HelpCmd::execute(const std::vector<std::string> &tokens)
{
	CommandController *cc = CommandController::instance();
	switch (tokens.size()) {
		case 1: {
			print("Use 'help [command]' to get help for a specific command");
			print("The following commands exists:");
			std::map<const std::string, Command*, ltstr>::const_iterator it;
			for (it=cc->commands.begin(); it!=cc->commands.end(); it++) {
				print(it->first);
			}
			break;
		}
		case 2: {
			std::map<const std::string, Command*, ltstr>::const_iterator it;
			it = cc->commands.find(tokens[1]);
			if (it == cc->commands.end())
				throw CommandException("Unknown command");
			std::vector<std::string> tokens2(tokens);
			tokens2.erase(tokens2.begin());
			it->second->help(tokens2);
			break;
		}
		default:
			throw CommandException("Syntax error");
	}
}
void CommandController::HelpCmd::help(const std::vector<std::string> &tokens)
{
	print("prints help information for commands");
}
void CommandController::HelpCmd::tabCompletion(std::vector<std::string> &tokens)
{
	std::string front = tokens.front();
	tokens.erase(tokens.begin());
	CommandController::instance()->tabCompletion(tokens);
	tokens.insert(tokens.begin(), front);
}


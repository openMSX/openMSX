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
#include "Command.hh"
#include "Console.hh"


CommandController::CommandController()
{
	registerCommand(helpCmd, "help");
}

CommandController::~CommandController()
{
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

void CommandController::unRegisterCommand(const std::string &str)
{
	assert(false);	// unimplemented
}


void CommandController::tokenize(const std::string &str, vector<std::string> &tokens, const std::string &delimiters = " ")
{
	// TODO implement "backslash before space"
	// Skip delimiters at beginning
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter"
	std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
	while (std::string::npos != pos || std::string::npos != lastPos) {
		// Found a token, add it to the vector
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
}

void CommandController::executeCommand(const std::string &cmd)
{
	std::vector<std::string> tokens;
	tokenize(cmd, tokens);
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
	std::vector<std::string>::const_iterator it=tokens.begin();
	if (it == tokens.end()) {
		command = std::string("");
	} else {
		command = *it; 
		it++;
		for (; it!=tokens.end(); it++) {
			command += ' ';
			command += *it;
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
		completeString(tokens[0], cmds);
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
	while (it!=list.end()) {
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
	while (true) {
		it = list.begin();
		if (string == *it) {
			// match is as long as first word
			return false;
		}
		// expand with one char 
		std::string string2 = string + (*it)[string.size()];
		for (;  it!=list.end(); it++) {
			if (string2 != (*it).substr(0, string2.size())) {
				return false;
			}
		}
		// no conflict found
		string = string2;
	}
}
void CommandController::completeString(std::string &string, std::list<std::string> &list)
{
	if (completeString2(string, list))
		string += " ";
}

void CommandController::completeFileName(std::string &filename)
{
	std::string npath, dpath;
	std::list<std::string> filenames;
	std::string::size_type pos = filename.find_last_of("/");	// TODO std delimiter
	if (pos==std::string::npos) {
		dpath = ".";
		npath = "";
	} else {
		dpath = filename.substr(0,pos);
		npath = filename.substr(0,pos+1);
	}
	
	DIR* dirp = opendir(dpath.c_str());
	if (dirp != NULL) {
		while (dirent* de = readdir(dirp)) {
			struct stat st;
			if (!(stat((npath+de->d_name).c_str(), &st))) {
				std::string name = npath + de->d_name;
				if (S_ISDIR(st.st_mode))
					name += "/";
				filenames.push_back(name);
			}
		}
	}
	bool t = completeString2(filename, filenames);
	if (t && filename[filename.size()-1]!='/')
		filename += " ";
}

// Help Command

void CommandController::HelpCmd::execute(const std::vector<std::string> &tokens)
{
	Console *console = Console::instance();
	CommandController *cc = CommandController::instance();
	if (tokens.size() == 1) {
		console->print("Use 'help [command]' to get help for a specific command");
		console->print("The following commands exists:");
		std::map<const std::string, Command*, ltstr>::const_iterator it;
		for (it=cc->commands.begin(); it!=cc->commands.end(); it++) {
			console->print(it->first);
		}
	} else {
		std::map<const std::string, Command*, ltstr>::const_iterator it;
		it = cc->commands.find(tokens[1]);
		if (it==cc->commands.end()) {
			console->print("Unknown command");
		} else {
			std::vector<std::string> tokens2(tokens);
			tokens2.erase(tokens2.begin());
			it->second->help(tokens2);
		}
	}
}
void CommandController::HelpCmd::help(const std::vector<std::string> &tokens)
{
	Console::instance()->print("prints help information for commands");
}
void CommandController::HelpCmd::tabCompletion(std::vector<std::string> &tokens)
{
	std::string front = tokens.front();
	tokens.erase(tokens.begin());
	CommandController::instance()->tabCompletion(tokens);
	tokens.insert(tokens.begin(), front);
}


// $Id$

#include <cassert>
#include "../openmsx.hh"
#include "../MSXConfig.hh"
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
	vector<std::string> tokens;
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

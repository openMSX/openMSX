// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <stdio.h>
#include <cassert>
#include <hash_map>
#include "../openmsx.hh"
#include "../MSXConfig.hh"
#include "Console.hh"
#include "ConsoleCommand.hh"


Console::Console()
{
}

Console::~Console()
{
	PRT_DEBUG("Destroying a Console object");
	
	for (int i=0; i<lineBuffer; i++) {
		free(consoleLines[i]);
		free(commandLines[i]);
	}
	free(consoleLines);
	free(commandLines);
}

Console *Console::instance()
{
	assert(oneInstance!=NULL);
	return oneInstance;
}
Console *Console::oneInstance = NULL;


void Console::registerCommand(ConsoleCommand &command, char *string)
{
	commands[string] = &command;
}

void Console::unRegisterCommand(char *commandString)
{
	assert(false);	// unimplemented
}


void Console::print(std::string text)
{
	int end = 0;
	bool more = true;
	while (more) {
		int start = end;
		end = text.find('\n', start);
		if (end == -1) {
			more = false;
			end = text.length();
		}
		char line[end - start + 1];
		text.copy(line, sizeof(line), start);
		line[sizeof(line) - 1] = '\0';
		out(line);
		end++; // skip newline
	}
}

void Console::out(const char *str, ...)
{
	va_list marker;
	char temp[256];

	va_start(marker, str);
	vsnprintf(temp, sizeof(temp), str, marker);
	va_end(marker);

	if (consoleLines) {
		strncpy(consoleLines[1], temp, CHARS_PER_LINE);
		consoleLines[1][CHARS_PER_LINE - 1] = '\0';
		newLineConsole();
		updateConsole();
	}
	PRT_DEBUG(temp);
}


// Increments the console line
void Console::newLineConsole()
{
	// Scroll
	char *temp = consoleLines[lineBuffer-1];
	for (int i = lineBuffer-1; i>1; i--) {
		consoleLines[i] = consoleLines[i-1];
	}
	consoleLines[1] = temp;

	memset(consoleLines[1], 0, CHARS_PER_LINE);
	if (totalConsoleLines < lineBuffer-1) {
		totalConsoleLines++;
	}
}

// Increments the command lines
void Console::newLineCommand()
{
	// Scroll.
	char *temp = commandLines[lineBuffer-1];
	for (int i = lineBuffer-1; i>0; i--) {
		commandLines[i] = commandLines[i-1];
	}
	commandLines[0] = temp;

	memset(commandLines[0], 0, CHARS_PER_LINE);
	if (totalCommands < lineBuffer-1) {
		totalCommands++;
	}
}


// executes the help command passed in from the string
void Console::commandHelp()
{
	char command[CHARS_PER_LINE];
	char *backStrings = consoleLines[0];

	// Get the command out of the string
	if (EOF == sscanf(backStrings, "%s", command))
		return;

	newLineConsole();

	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	it = commands.find(command);
	if (it==commands.end()) {
		out("No help for command");
	} else {
		it->second->help(backStrings);
	}
}

void Console::commandExecute(const std::string cmd)
{
	const char *backStrings = cmd.c_str();
	// Get the command out of the string
	char command[CHARS_PER_LINE];
	if (EOF == sscanf(backStrings, "%s", command))
		return;

	newLineConsole();
	
	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	it = commands.find(command);
	if (it==commands.end()) {
		out("Unknown command");
	} else {
		it->second->execute(backStrings);
	}
}

void Console::autoCommands()
{
	try {
		MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("AutoCommands");
		std::list<MSXConfig::Device::Parameter*>* commandList;
		commandList = config->getParametersWithClass("");
		std::list<MSXConfig::Device::Parameter*>::const_iterator i;
		for (i = commandList->begin(); i != commandList->end(); i++) {
			commandExecute((*i)->value);
		}
	} catch (MSXConfig::Exception &e) {
		// no auto commands defined
	}
}

/* tab completes commands
 * It takes a string to complete and a pointer to the strings length. The strings
 * length will be modified if the command is completed. */
void Console::tabCompletion()
{
	int matches = 0;
	int *location = &stringLocation;
	char *commandLine = consoleLines[0];
	const char *matchingCommand;
	int commandlength;
	int spacefound = 0;
	
	// need to check only up until the first space otherwise 
	// the user is typing options, which will need an extra routine :-)
	// if there is a space then the command must be already completed
	PRT_DEBUG(commandLine);
	for (int i=0;i<strlen(commandLine);i++){
		//printf("%i : %c %i \n", i, CommandLine[i], (int)CommandLine[i] );
		if (commandLine[i] == ' '){
			spacefound=i;
			break;
		}
	};
	PRT_DEBUG(commandLine);

	// Find all the commands that match
	commandlength = spacefound ? spacefound : strlen(commandLine);
	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	for (it=commands.begin(); it!=commands.end(); it++) {
		if (0 == strncmp(commandLine, it->first, commandlength)) {
			matchingCommand=it->first;
			matches++;
		}
	}
	
	if (matches == 1) {
		// if we got one match, we select it
		if (spacefound) {
			// print the help of this command
			newLineCommand();
			commandHelp();
		} else {
			strcpy(commandLine, matchingCommand);
			commandLine[strlen(matchingCommand)] = ' ';
			*location = strlen(matchingCommand)+1;
			updateConsole();
		}
	}
	else if (matches > 1) {
	        // multiple matches so print them out to the user
		newLineConsole();
		for (it=commands.begin(); it!=commands.end(); it++) {
			if (0 == strncmp(commandLine, it->first, strlen(commandLine)))
				out(it->first);
		}
	}
	else if (matches == 0)
		out("No matching command found!");
}


// Lists all the commands to be used in the console
void Console::listCommands()
{
	out(" ");
	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	for (it=commands.begin(); it!=commands.end(); it++) {
		out("%s", it->first);
	}
}

// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <stdio.h>
#include <cassert>
#include "../openmsx.hh"
#include "../MSXConfig.hh"
#include "Console.hh"
#include "CommandController.hh"
#include "Command.hh"



Console::Console()
{
	totalConsoleLines = 0;
	consoleScrollBack = 0;
	totalCommands = 0;
	stringLocation = 0;
	commandScrollBack = 0;

	consoleLines = (char**)malloc(sizeof(char*) * NUM_LINES);
	commandLines = (char**)malloc(sizeof(char*) * NUM_LINES);
	for (int loop=0; loop <= NUM_LINES-1; loop++) {
		consoleLines[loop] = (char *)calloc(CHARS_PER_LINE, sizeof(char));
		commandLines[loop] = (char *)calloc(CHARS_PER_LINE, sizeof(char));
	}
}

Console::~Console()
{
	PRT_DEBUG("Destroying a Console object");
	
	for (int i=0; i<NUM_LINES; i++) {
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


void Console::print(const std::string &text)
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
#ifndef __WIN32__
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
#endif
}


// Increments the console line
void Console::newLineConsole()
{
	// Scroll
	char *temp = consoleLines[NUM_LINES-1];
	for (int i = NUM_LINES-1; i>1; i--) {
		consoleLines[i] = consoleLines[i-1];
	}
	consoleLines[1] = temp;

	memset(consoleLines[1], 0, CHARS_PER_LINE);
	if (totalConsoleLines < NUM_LINES-1) {
		totalConsoleLines++;
	}
}

// Increments the command lines
void Console::newLineCommand()
{
	// Scroll
	char *temp = commandLines[NUM_LINES-1];
	for (int i = NUM_LINES-1; i>0; i--) {
		commandLines[i] = commandLines[i-1];
	}
	commandLines[0] = temp;

	memset(commandLines[0], 0, CHARS_PER_LINE);
	if (totalCommands < NUM_LINES-1) {
		totalCommands++;
	}
}


// executes the help command passed in from the string
void Console::commandHelp()
{
	print("Help currently broken");
	/*
	std::string cmd(consoleLines[0]);
	vector<std::string> tokens;
	tokenize(cmd, tokens);
	if (tokens.empty())
		return;
	
	newLineConsole();
	std::map<const std::string, ConsoleCommand*, ltstr>::const_iterator it;
	it = commands.find(tokens[0]);
	if (it==commands.end()) {
		print("No help for command");
	} else {
		it->second->help(tokens);
	}
	*/
}

void Console::commandExecute(const std::string &cmd)
{
	newLineConsole();
	try {
		CommandController::instance()->executeCommand(cmd);
	} catch (CommandException &e) {
		print(e.desc);
	}
}

/* tab completes commands
 * It takes a string to complete and a pointer to the strings length. The strings
 * length will be modified if the command is completed. */
void Console::tabCompletion()
{
	/*
	int matches = 0;
	int *location = &stringLocation;
	char *commandLine = consoleLines[0];
	const char *matchingCommand = 0;
	int commandlength;
	int spacefound = 0;
	
	// need to check only up until the first space otherwise 
	// the user is typing options, which will need an extra routine :-)
	// if there is a space then the command must be already completed
	PRT_DEBUG(commandLine);
	for (unsigned i=0; i<strlen(commandLine); i++){
		//printf("%i : %c %i \n", i, CommandLine[i], (int)CommandLine[i] );
		if (commandLine[i] == ' '){
			spacefound=i;
			break;
		}
	};
	PRT_DEBUG(commandLine);

	// Find all the commands that match
	commandlength = spacefound ? spacefound : strlen(commandLine);
	std::map<const std::string, ConsoleCommand*, ltstr>::const_iterator it;
	for (it=commands.begin(); it!=commands.end(); it++) {
		if (0 == strncmp(commandLine, it->first.c_str(), commandlength)) {
			matchingCommand=it->first.c_str();
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
			if (0 == strncmp(commandLine, it->first.c_str(), strlen(commandLine)))
				print(it->first);
		}
	}
	else if (matches == 0)
		print("No matching command found!");
	*/
	print("TAB completion currently broken");
}

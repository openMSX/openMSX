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
	cursorLocation = 0;
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
	
		strncpy(consoleLines[1], line, CHARS_PER_LINE);
		consoleLines[1][CHARS_PER_LINE - 1] = '\0';
		newLineConsole();
		updateConsole();
		PRT_DEBUG(line);
	
	end++; // skip newline
	}
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


void Console::commandExecute(const std::string &cmd)
{
	newLineConsole();
	try {
		CommandController::instance()->executeCommand(cmd);
	} catch (CommandException &e) {
		print(e.desc);
	}
}

void Console::tabCompletion()
{
	std::string string(consoleLines[0]);
	CommandController::instance()->tabCompletion(string);
	strncpy(consoleLines[0], string.c_str(), CHARS_PER_LINE);
	cursorLocation = string.length();
	updateConsole();
}

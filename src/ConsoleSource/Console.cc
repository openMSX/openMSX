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
	totalCommands = 0;

	consoleLines = (char**)malloc(sizeof(char*) * NUM_LINES);
	commandLines = (char**)malloc(sizeof(char*) * NUM_LINES);
	for (int loop=0; loop <= NUM_LINES-1; loop++) {
		consoleLines[loop] = (char *)calloc(CHARS_PER_LINE, sizeof(char));
		commandLines[loop] = (char *)calloc(CHARS_PER_LINE, sizeof(char));
	}
	putPrompt();
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
		char line[end-start+1];
		text.copy(line, sizeof(line), start);
		line[sizeof(line)-1] = '\0';
		strncpy(consoleLines[0], line, CHARS_PER_LINE);
		consoleLines[0][CHARS_PER_LINE-1] = '\0';
		newLineConsole();
		updateConsole();
		end++; // skip newline
		PRT_DEBUG(line);
	}
}

// Increments the console line
void Console::newLineConsole()
{
	// Scroll
	char *temp = consoleLines[NUM_LINES-1];
	for (int i=NUM_LINES-1; i>0; i--) {
		consoleLines[i] = consoleLines[i-1];
	}
	consoleLines[0] = temp;

	memset(consoleLines[0], 0, CHARS_PER_LINE);
	if (totalConsoleLines < NUM_LINES-1) {
		totalConsoleLines++;
	}
	cursorLocation = 0;
}

// Increments the command lines
void Console::putCommandHistory(char* command)
{
	// Scroll
	char *temp = commandLines[NUM_LINES-1];
	for (int i = NUM_LINES-1; i>0; i--) {
		commandLines[i] = commandLines[i-1];
	}
	commandLines[0] = temp;

	strcpy(commandLines[0], command);
	if (totalCommands < NUM_LINES-1) {
		totalCommands++;
	}
}

void Console::commandExecute()
{
	putCommandHistory(consoleLines[0]);
	newLineConsole();
	try {
		CommandController::instance()->
			executeCommand(std::string(commandLines[0]+PROMPT_SIZE));
	} catch (CommandException &e) {
		print(e.desc);
	}
	putPrompt();
}

void Console::putPrompt()
{
	consoleScrollBack = 0;
	commandScrollBack = -1;
	consoleLines[0][0] = '>';
	consoleLines[0][1] = ' ';
	memset(consoleLines[0]+PROMPT_SIZE, 0, CHARS_PER_LINE-PROMPT_SIZE);
	cursorLocation = PROMPT_SIZE;
}

void Console::tabCompletion()
{
	std::string string(consoleLines[0]+PROMPT_SIZE);
	CommandController::instance()->tabCompletion(string);
	strncpy(consoleLines[0]+PROMPT_SIZE, string.c_str(), CHARS_PER_LINE-PROMPT_SIZE);
	cursorLocation = string.length()+PROMPT_SIZE;
}

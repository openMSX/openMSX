// $Id$

/*  CON_consolecommands.c
 *  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hash_map>
#include "SDL.h"
#include "CON_console.hh"
#include "CON_consolecommands.hh"
#include "CON_internal.hh"



struct eqstr {
	bool operator()(const char* s1, const char* s2) const {
		return strcmp(s1, s2) == 0;
	}
};

std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr> commands;


// executes the help command passed in from the string
void CON_CommandHelp(ConsoleInformation *console)
{
	char command[CON_CHARS_PER_LINE];
	char *backStrings = console->ConsoleLines[0];

	// Get the command out of the string
	if (EOF == sscanf(backStrings, "%s", command))
		return;

	CON_NewLineConsole(console);

	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	it = commands.find(command);
	if (it==commands.end()) {
		CON_Out(console, "No help for command");
	} else {
		it->second->help(backStrings);
	}
}

// executes the command passed in from the string
void CON_CommandExecute(ConsoleInformation *console)
{
	char command[CON_CHARS_PER_LINE];
	char *backStrings = console->ConsoleLines[0];
	
	// Get the command out of the string
	if (EOF == sscanf(backStrings, "%s", command))
		return;

	CON_NewLineConsole(console);
	
	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	it = commands.find(command);
	if (it==commands.end()) {
		CON_Out(console, "Unknown command");
	} else {
		it->second->execute(backStrings);
	}
}

/* Use this to add commands.
 * Pass in a pointer to a funtion that will take a string which contains the 
 * arguments passed to the command. Second parameter is the command to execute
 * on.
 */
void CON_AddCommand(ConsoleCommand *callableObject, const char *commandWord)
{
	commands[commandWord] = callableObject;
}

/* tab completes commands
 * It takes a string to complete and a pointer to the strings length. The strings
 * length will be modified if the command is completed. */
void CON_TabCompletion(ConsoleInformation *console)
{
	int Matches = 0;
	int *location = &console->StringLocation;
	char *CommandLine = console->ConsoleLines[0];
	const char *MatchingCommand;
	int commandlength;
	int spacefound = 0;
	
	// need to check only up until the first space otherwise 
	// the user is typing options, which will need an extra routine :-)
	// if there is a space then the command must be already completed
	printf("%s \n", CommandLine );
	for (int i=0;i<strlen(CommandLine);i++){
		//printf("%i : %c %i \n", i, CommandLine[i], (int)CommandLine[i] );
		if (CommandLine[i] == ' '){
			spacefound=i;
			break;
		}
	};
	printf("%s \n", CommandLine );

	// Find all the commands that match
	commandlength = spacefound ? spacefound : strlen(CommandLine);
	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	for (it=commands.begin(); it!=commands.end(); it++) {
		if (0 == strncmp(CommandLine, it->first, commandlength)) {
			MatchingCommand=it->first;
			Matches++;
		}
	}
	
	if (Matches == 1) {
		// if we got one match, we select it
		if (spacefound) {
			// print the help of this command
			CON_NewLineCommand(console);
			CON_CommandHelp(console);
		} else {
			strcpy(CommandLine, MatchingCommand);
			CommandLine[strlen(MatchingCommand)] = ' ';
			*location = strlen(MatchingCommand)+1;
			CON_UpdateConsole(console);
		}
	}
	else if (Matches > 1) {
	        // multiple matches so print them out to the user
		CON_NewLineConsole(console);
		for (it=commands.begin(); it!=commands.end(); it++) {
			if (0 == strncmp(CommandLine, it->first, strlen(CommandLine)))
				CON_Out(console, it->first);
		}
	}
	else if (Matches == 0)
		CON_Out(console, "No matching command found!");
}


// Lists all the commands to be used in the console
void CON_ListCommands(ConsoleInformation *console)
{
	CON_Out(console, " ");
	std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr>::const_iterator it;
	for (it=commands.begin(); it!=commands.end(); it++) {
		CON_Out(console, "%s", it->first);
	}
}


// This frees all the memory used by the commands
void CON_DestroyCommands()
{
	commands.clear();
}

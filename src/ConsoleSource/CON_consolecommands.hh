// $Id$

#ifndef __CON_CONSOLECOMMANDS_HH__
#define __CON_CONSOLECOMMANDS_HH__

#include "CON_console.hh"
#include "../ConsoleInterface.hh"

typedef struct CommandInfo_td
{
	//void			(*CommandCallback)(ConsoleInformation *console, char *Parameters);
	ConsoleInterface *tocall;
	char			*CommandWord;
	struct CommandInfo_td	*NextCommand;
} CommandInfo;

void	CON_CommandExecute(ConsoleInformation *console);
void	CON_CommandHelp(ConsoleInformation *console);
void	CON_AddCommand(ConsoleInterface *callableObject , const char *CommandWord);
void	CON_TabCompletion(ConsoleInformation *console);
void	CON_ListCommands(ConsoleInformation *console);
void	CON_DestroyCommands();

#endif

// $Id$

#ifndef __CON_CONSOLECOMMANDS_HH__
#define __CON_CONSOLECOMMANDS_HH__

#include "CON_console.hh"
#include "../ConsoleCommand.hh"


void	CON_CommandExecute(ConsoleInformation *console);
void	CON_CommandHelp(ConsoleInformation *console);
void	CON_AddCommand(ConsoleCommand *callableObject , const char *CommandWord);
void	CON_TabCompletion(ConsoleInformation *console);
void	CON_ListCommands(ConsoleInformation *console);
void	CON_DestroyCommands();

#endif

#ifndef CON_consolecommands_H
#define CON_consolecommands_H

#include "CON_console.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CommandInfo_td
{
	void			(*CommandCallback)(ConsoleInformation *console, char *Parameters);
	char			*CommandWord;
	struct CommandInfo_td	*NextCommand;
} CommandInfo;


void	CON_SendFullCommand(int sendOn);
void	CON_CommandExecute(ConsoleInformation *console);
void	CON_AddCommand(void (*CommandCallback)(ConsoleInformation *console, char *Parameters), const char *CommandWord);
void	CON_TabCompletion(ConsoleInformation *console);
void	CON_ListCommands(ConsoleInformation *console);
void	CON_DestroyCommands();

#ifdef __cplusplus
};
#endif

#endif

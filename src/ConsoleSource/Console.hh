// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <string>
#include <hash_map>


class ConsoleCommand;

class Console
{
	public:
		virtual ~Console();
		static Console* instance();

		/**
		 * objects that want to accept commands from the console 
		 * must register themself
		 */
		void registerCommand(ConsoleCommand &commandObject, char *string);
		void unRegisterCommand(char *commandString);
		
		/**
		 * Prints a string on the console
		 */
		void print(std::string text);
		
		/**
		 * This method redraws console, it must be called every frame
		 */
		virtual void drawConsole() = 0;
		
		/**
		 * Executes all defined auto commands
		 */
		void autoCommands();
		
	protected:
		Console();
		void tabCompletion();
		void newLineCommand();
		void commandExecute(const char *backstrings);
		void listCommands();
		void commandHelp();
		void out(const char *str, ...);
		void newLineConsole();
		virtual void updateConsole() = 0;
		
		static const int CHARS_PER_LINE = 128;
	
		static Console* oneInstance;
		
		/** List of all the past lines.
		  */
		char **consoleLines;

		/** List of all the past commands.
		  * TODO: Call this "history" or "commandHistory".
		  */
		char **commandLines;

		/** Total number of lines in the console.
		  */
		int totalConsoleLines;

		/** How much the users scrolled back in the console.
		  */
		int consoleScrollBack;

		/** Number of commands in the Back Commands.
		  */
		int totalCommands;

		/** The number of lines in the console.
		  */
		int lineBuffer;

		/** Current character location in the current string.
		  */
		int stringLocation;

		/** How much the users scrolled back in the command lines.
		  */
		int commandScrollBack;
		
	private:
		struct eqstr {
			bool operator()(const char* s1, const char* s2) const {
				return strcmp(s1, s2) == 0;
			}
		};
		std::hash_map<const char*, ConsoleCommand*, hash<const char*>, eqstr> commands;
};

#endif

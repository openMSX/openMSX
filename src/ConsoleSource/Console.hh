// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <string>
#include <map>
#include <vector>


class ConsoleCommand;

class Console
{
	public:
		virtual ~Console();
		static Console* instance();

		/**
		 * Prints a string on the console
		 */
		void print(const std::string &text);
		
		/**
		 * This method redraws console, it must be called every frame
		 */
		virtual void drawConsole() = 0;
		
	protected:
		Console();
		void tabCompletion();
		void newLineCommand();
		void commandHelp();
		void commandExecute(const std::string &cmd);
		void out(const char *str, ...);
		void newLineConsole();
		virtual void updateConsole() = 0;
		
		static const int CHARS_PER_LINE = 128;
		static const int NUM_LINES = 100;

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

		/** Current character location in the current string.
		  */
		int stringLocation;

		/** How much the users scrolled back in the command lines.
		  */
		int commandScrollBack;
};

#endif

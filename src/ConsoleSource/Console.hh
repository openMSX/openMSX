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
		 * objects that want to accept commands from the console 
		 * must register themself
		 */
		void registerCommand(ConsoleCommand &commandObject, const std::string &str);
		void unRegisterCommand(const std::string &str);
		
		/**
		 * Prints a string on the console
		 */
		void print(const std::string &text);
		
		/**
		 * This method redraws console, it must be called every frame
		 */
		virtual void drawConsole() = 0;
		
		/**
		 * Executes all defined auto commands
		 */
		void autoCommands();
		
		/**
		 * Execute a given command
		 */
		void commandExecute(const std::string &backstrings);
		
	protected:
		Console();
		void tabCompletion();
		void newLineCommand();
		void listCommands();
		void commandHelp();
		void out(const char *str, ...);
		void newLineConsole();
		virtual void updateConsole() = 0;
		void tokenize(const std::string &str, vector<std::string> &tokens, const std::string &delimiters = " ");
		
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
		struct ltstr {
			bool operator()(const std::string &s1, const std::string &s2) const {
				return s1 < s2;
			}
		};
		//struct eqstr {
		//	bool operator()(const std::string &s1, const std::string &s2) const {
		//		return s1 == s2;
		//	}
		//};
		std::map<const std::string, ConsoleCommand*, ltstr> commands;
};

#endif

// $Id$

#ifndef __CONSOLEMANAGER_HH__
#define __CONSOLEMANAGER_HH__

#include <string>
#include <vector>

// Forward declarations
class Console;


class ConsoleManager
{
	public:
		static ConsoleManager* instance();

		/**
		 * Prints a string on all the consoles
		 */
		void print(const std::string &text);

		/**
		 * Register a new Console
		 */
		void registerConsole(Console *console);
		
	private:
		static ConsoleManager* oneInstance;
		std::vector<Console*> consoles;
};

#endif

// $Id$

#ifndef __CONSOLEMANAGER_HH__
#define __CONSOLEMANAGER_HH__

#include <string>
#include <list>

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
		 * (Un)register a Console
		 */
		void registerConsole(Console *console);
		void unregisterConsole(Console *console);
		
	private:
		static ConsoleManager* oneInstance;
		std::list<Console*> consoles;
};

#endif

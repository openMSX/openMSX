// $Id$

#ifndef __CONSOLECOMMAND_HH__
#define __CONSOLECOMMAND_HH__

#include <string>
#include <vector>


/** These are the functions the console can call after a device
  * has registered commands with the console.
  */
class ConsoleCommand
{
	public:
		/**
		 * called by the console when a command is typed
		 */
		virtual void execute(const std::vector<std::string> &tokens)=0;
		/**
		 * called by the console when a help command is typed
		 */
		virtual void help   (const std::vector<std::string> &tokens)=0;
};

#endif //__CONSOLECOMMAND_HH__

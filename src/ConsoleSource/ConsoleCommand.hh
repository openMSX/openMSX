// $Id$

#ifndef __CONSOLECOMMAND_HH__
#define __CONSOLECOMMAND_HH__

/** These are the functions the console can call after a device
  * has registered commands with the console.
  */
class ConsoleCommand
{
	public:
		/**
		 * called by the console when a command is typed
		 */
		virtual void execute(const char *string)=0;
		/**
		 * called by the console when a help command is typed
		 */
		virtual void help(const char *string)=0;
};

#endif //__CONSOLECOMMAND_HH__

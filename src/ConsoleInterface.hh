// $Id$
//
// These are the functions the console can call after a device 
// has registered cmmands with the console.
//

#ifndef __CONSOLEINTERFACE_HH__
#define __CONSOLEINTERFACE_HH__

class ConsoleInterface
{
	public:
		/**
		 * called by the console when a command is typed
		 */
		virtual void ConsoleCallback(char *string);
		/**
		 * called by the console when a help command is typed
		 */
		virtual void ConsoleHelp();
};

#endif //__MSXCONSOLEINTERFACE_HH__

// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <string>


class Console
{
	public:
		Console();
		virtual ~Console();

		/**
		 * Prints a string on the console
		 */
		virtual void print(const std::string &text) = 0;
};

#endif

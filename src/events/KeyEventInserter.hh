// $Id$

#ifndef __KEYEVENTINSERTER_HH__
#define __KEYEVENTINSERTER_HH__

#include <string>
#include <SDL/SDL.h>
#include "CommandLineParser.hh"


class KeyEventInserterCLI : public CLIOption
{
	public:
		KeyEventInserterCLI();
		virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
		virtual const std::string& optionHelp();
};


class KeyEventInserter
{
	public:
		KeyEventInserter();
		KeyEventInserter &operator<<(const std::string &str);
		KeyEventInserter &operator<<(const char* cstr);
		void enter(const std::string &str);

	private:
		static const SDLKey keymap[256][4];
};

#endif

// $Id$

#ifndef __KEYEVENTINSERTER_HH__
#define __KEYEVENTINSERTER_HH__

#include <string>
#include <SDL/SDL.h>


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

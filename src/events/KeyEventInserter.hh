// $Id$

#ifndef __KEYEVENTINSERTER_HH__
#define __KEYEVENTINSERTER_HH__

#include <string>
#include <SDL/SDL.h>  // TODO get rid of this

using std::string;

namespace openmsx {

class EmuTime;

class KeyEventInserter
{
public:
	KeyEventInserter();
	void enter(const string& str, const EmuTime& time);

private:
	static const SDLKey keymap[256][4];
};

} // namespace openmsx

#endif

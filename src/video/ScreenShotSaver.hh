// $Id$

#ifndef __SCREENSHOTSAVER_HH__
#define __SCREENSHOTSAVER_HH__

#include <string>
#include <SDL/SDL.h>
#include "openmsx.hh"
#include "CommandException.hh"

using std::string;

namespace openmsx {

/** Utility class to hide the complexity of saving to a .png file
  */
class ScreenShotSaver
{
public:
	ScreenShotSaver(SDL_Surface* image, const string& filename)
		throw (CommandException);
	ScreenShotSaver(unsigned witdh, unsigned height,
	                byte** row_pointers, const string& filename)
		throw (CommandException);
};

} // namespace openmsx

#endif // __SCREENSHOTSAVER_HH__

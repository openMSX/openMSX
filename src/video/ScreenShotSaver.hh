// $Id$

#ifndef __SCREENSHOTSAVER_HH__
#define __SCREENSHOTSAVER_HH__

#include <string>
#include <SDL/SDL.h>

using std::string;

namespace openmsx {

/** Utility class to hide the complexity of saving to a .png file
  */
class ScreenShotSaver
{
public:
	ScreenShotSaver(SDL_Surface* image);
	void take(const string& filename);

private:
	SDL_Surface* image;
};

} // namespace openmsx

#endif // __SCREENSHOTSAVER_HH__

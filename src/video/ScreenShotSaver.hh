// $Id$

#ifndef __SCREENSHOTSAVER_HH__
#define __SCREENSHOTSAVER_HH__

#include <SDL/SDL.h>

namespace openmsx {

/** Utility class to hide the complexity of saving to a .png file
  */
class ScreenShotSaver
{
public:
	ScreenShotSaver(SDL_Surface* image);
	void take();

private:
	ScreenShotSaver() {};

	SDL_Surface* image;

	static int count;
};

} // namespace openmsx

#endif // __SCREENSHOTSAVER_HH__

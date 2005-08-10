// $Id$

#ifndef SCREENSHOTSAVER_HH
#define SCREENSHOTSAVER_HH

#include <string>
#include "openmsx.hh"

struct SDL_Surface;

namespace openmsx {

/** Utility class to hide the complexity of saving to a .png file
  */
class ScreenShotSaver
{
public:
	static void save(SDL_Surface* image, const std::string& filename);
	static void save(unsigned witdh, unsigned height,
	                 byte** row_pointers, const std::string& filename);
};

} // namespace openmsx

#endif
